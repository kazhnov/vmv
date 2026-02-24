#include "redist/vgfx/vgfx.h"
#include "redist/vvox/vvox.h"
#define _VMATH_IMPLEMENTATION_
#define _VCOLOR_IMPLEMENTATION_
#include "redist/vgfx/include/vmath/vmath.h"
#include "redist/vgfx/include/vcolor/vcolor.h"
#include "redist/vgfx/include/vshape/vshape.h"
#include <stddef.h>

static Camera* camera;
static f32 angles[2] = {0, 0};
static b8 show_original = 0;

void iVMV_InputUpdate();

int main() {
    VG_WindowOpen("Vector Model Viewer", (f32[]){1280, 720}, 0);
    VG_BackgroundColorSet((f32[]){0.5, 0.5, 0.5, 1});
    VG_VSyncSet(true);
    u32 texture_default = VG_TextureNew("redist/vgfx/include/vtex/textures/default.ppm");
    VG_TextureDefaultSet(texture_default);
    u32 shader_default = VG_ShaderLoad("redist/vgfx/shaders/shader.vert", "redist/vgfx/shaders/shader.frag");
    u32 model = VG_ModelNew("redist/vgfx/models/bunny.obj", 0 , shader_default);
    u32 model_floor = VG_ModelNew("redist/vgfx/models/floor.obj", 0, shader_default);
    u32 model_cube = VG_ModelNew("models/cube.obj", 0, shader_default);
    VG_ModelColorSet(model, VRGBA_WHITE);
    camera = VG_CameraGet();
    VM3_Copy(camera->position, VM3_ZERO);
    VM3_Copy(camera->rotation, VM3_ZERO);
    
    u32 light_red_handle = VG_DirectLightCreate();
    DirectLight* light_red = VG_DirectLightGet(light_red_handle);
    VM3_Copy(light_red->direction, (f32[3]){-1, 0, 0});
    VRGB_Copy(light_red->color, VRGB_RED);
    
    u32 light_green_handle = VG_DirectLightCreate();
    DirectLight* light_green = VG_DirectLightGet(light_green_handle);
    VM3_Copy(light_green->direction, (f32[3]){0, -1, 0});
    VRGB_Copy(light_green->color, VRGB_GREEN);
    
    u32 light_blue_handle = VG_DirectLightCreate();
    DirectLight* light_blue = VG_DirectLightGet(light_blue_handle);
    VM3_Copy(light_blue->direction, (f32[3]){0, 0, -1});
    VRGB_Copy(light_blue->color, VRGB_BLUE);
    
    Mesh* mesh_bunny = malloc(sizeof(Mesh));
    Mesh* mesh_cube = malloc(sizeof(Mesh));;
    VMESH_LoadObj(mesh_bunny, "redist/vgfx/models/bunny.obj");
    VMESH_LoadObj(mesh_cube, "models/cube.obj");
    f32 voxel_size = 0.02;
    u32 voxel_count[3];
    b8* voxels = VVOX_VerticesToVoxels(
	mesh_bunny->vertices, mesh_bunny->vertex_count, sizeof(Vertex), offsetof(Vertex, pos),
	mesh_bunny->indices, mesh_bunny->index_count, voxel_size, voxel_count
	);

    Mesh* mesh_voxelized = calloc(1, sizeof(Mesh));
    u32 filled_voxel_count = 0;
    for (int i = 0; i < voxel_count[0]*voxel_count[1]*voxel_count[2]; i++) {
	if (voxels[i]) filled_voxel_count++;
    }
    
    mesh_voxelized->vertices = calloc(filled_voxel_count*mesh_cube->vertex_count, sizeof(Vertex));
    mesh_voxelized->indices = calloc(filled_voxel_count*mesh_cube->index_count, sizeof(u32));
    mesh_voxelized->vertex_count = 0;
    mesh_voxelized->index_count = 0;

    VShape mesh_box = VSHAPE_BoxFromVertices(mesh_bunny->vertices,
					mesh_bunny->vertex_count,
					sizeof(Vertex),
					offsetof(Vertex, pos));
    f32 box_min[3]; f32 box_halfsize[3];
    VM3_ScaleO(mesh_box.size, 0.5, box_halfsize);
    VM3_SubtractO(mesh_box.center, box_halfsize, box_min);
    
    u32 voxel_current = 0;
    for (u32 i = 0; i < voxel_count[0]; i++) {
	for (u32 j = 0; j < voxel_count[1]; j++) {
	    for (u32 k = 0; k < voxel_count[2]; k++) {
		if (!voxels[k*voxel_count[0]*voxel_count[1] + j*voxel_count[0] + i]) continue;
		f32 voxel_position[3];
		VM3_Set(voxel_position, i*voxel_size, j*voxel_size, k*voxel_size);
		VM3_Add(voxel_position, box_min);
		for (u32 vertex = 0; vertex < mesh_cube->vertex_count; vertex++) {
		    Vertex* vertex_out = mesh_voxelized->vertices + mesh_voxelized->vertex_count;
		    Vertex* vertex_in = mesh_cube->vertices + vertex;
		    VM3_ScaleO(vertex_in->pos, voxel_size, vertex_out->pos);
		    VM3_Add(vertex_out->pos, voxel_position);
		    VM3_Copy(vertex_out->normal, vertex_in->normal);
		    mesh_voxelized->vertex_count++;
		}
		for (u32 index = 0; index < mesh_cube->index_count; index++) {
		    mesh_voxelized->indices[mesh_voxelized->index_count] = mesh_cube->indices[index]+voxel_current*mesh_cube->vertex_count;
		    mesh_voxelized->index_count++;
		}
		voxel_current++;
	    }
	}
    }

    u32 model_bunny_voxelized = VG_ModelFromMesh(mesh_voxelized, 0, shader_default);
    VMESH_Destroy(mesh_voxelized);
    VMESH_Destroy(mesh_cube);
    VMESH_Destroy(mesh_bunny);

    char fps_string[50];
    while(!VG_WindowShouldClose()) {
	snprintf(fps_string, 49, "VModelViewer. FPS: %f", VG_FPSGet());
	VG_WindowTitleSet(fps_string);
	
	VG_DrawingBegin();
	iVMV_InputUpdate();
	if (show_original) {
	    VG_ModelDrawAt(model, VM3_FORWARD, VM3_ZERO, VM3_ONE);
	}
	VG_ModelDrawAt(model_bunny_voxelized, VM3_FORWARD, VM3_ZERO, VM3_ONE);
//	VG_ModelDrawAt(model_floor, VM3_DOWN, VM3_ZERO, (f32[]){10, 10, 10});
	VG_DrawingEnd();
    }

    VG_WindowClose();
    return 0;
}

void iVMV_InputUpdate() {
    f32 mouse_delta[2];
    VG_MouseDeltaGet(mouse_delta);
    f32 dt = VG_DeltaTimeGet();
    
    f32 camera_position[3];
    VG_CameraPositionGet(camera_position);
    
    if (VG_KeyDown(VG_KEY_W)) {
	camera_position[2] -= dt;
    }
    
    if (VG_KeyDown(VG_KEY_S)) {
	camera_position[2] += dt;
    }

    if (VG_KeyPressed(VG_KEY_SPACE)) {
	show_original = !show_original;
    }
    
//    camera_position[2] = 5;
    
    VG_CameraPositionSet(camera_position);

    
}
