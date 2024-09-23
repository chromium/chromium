// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/draw_quad.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/unguessable_token.h"
#include "cc/base/math_util.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/fake_raster_source.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"

using testing::ElementsAreArray;

namespace viz {
namespace {

using RoundedDisplayMasksInfo = TextureDrawQuad::RoundedDisplayMasksInfo;

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

ResourceId NextId(ResourceId id) {
  return ResourceId(id.GetUnsafeValue() + 1);
}

TEST(DrawQuadTest, CopySharedQuadState) {
  constexpr gfx::Transform quad_transform =
      gfx::Transform::Affine(1.0, 0.5, 0.0, 1.0, 0.5, 0.0);
  constexpr gfx::Rect layer_rect(26, 28);
  const gfx::MaskFilterInfo mask_filter_rounded_corners(
      gfx::RectF(5, 5), gfx::RoundedCornersF(2.5), gfx::LinearGradient());
  constexpr gfx::Rect visible_layer_rect(10, 12, 14, 16);
  constexpr gfx::Rect clip_rect(19, 21, 23, 25);
  constexpr bool are_contents_opaque = true;
  constexpr float opacity = 0.25f;
  constexpr SkBlendMode blend_mode = SkBlendMode::kMultiply;
  constexpr int sorting_context_id = 65536;
  constexpr uint32_t layer_id = 0u;
  constexpr bool is_fast_rounded_corner = true;

  auto state = std::make_unique<SharedQuadState>();
  state->SetAll(quad_transform, layer_rect, visible_layer_rect,
                mask_filter_rounded_corners, clip_rect, are_contents_opaque,
                opacity, blend_mode, sorting_context_id, layer_id,
                is_fast_rounded_corner);

  auto copy = std::make_unique<SharedQuadState>(*state);
  EXPECT_EQ(quad_transform, copy->quad_to_target_transform);
  EXPECT_EQ(visible_layer_rect, copy->visible_quad_layer_rect);
  EXPECT_EQ(mask_filter_rounded_corners, copy->mask_filter_info);
  EXPECT_EQ(opacity, copy->opacity);
  EXPECT_EQ(clip_rect, copy->clip_rect);
  EXPECT_EQ(are_contents_opaque, copy->are_contents_opaque);
  EXPECT_EQ(blend_mode, copy->blend_mode);
  EXPECT_EQ(layer_id, copy->layer_id);
  EXPECT_EQ(is_fast_rounded_corner, copy->is_fast_rounded_corner);
}

SharedQuadState* CreateSharedQuadState(CompositorRenderPass* render_pass) {
  constexpr gfx::Transform quad_transform =
      gfx::Transform::Affine(1.0, 0.5, 0.0, 1.0, 0.5, 0.0);
  constexpr gfx::Rect layer_rect(26, 28);
  constexpr gfx::Rect visible_layer_rect(10, 12, 14, 16);
  constexpr bool are_contents_opaque = true;
  constexpr float opacity = 1.f;
  constexpr int sorting_context_id = 65536;
  constexpr SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  constexpr bool is_fast_rounded_corner = false;
  constexpr uint32_t layer_id = 0u;

  SharedQuadState* state = render_pass->CreateAndAppendSharedQuadState();
  state->SetAll(quad_transform, layer_rect, visible_layer_rect,
                gfx::MaskFilterInfo(), std::nullopt, are_contents_opaque,
                opacity, blend_mode, sorting_context_id, layer_id,
                is_fast_rounded_corner);
  return state;
}

void CompareSharedQuadState(const SharedQuadState* source_sqs,
                            const SharedQuadState* copy_sqs) {
  EXPECT_EQ(source_sqs->quad_to_target_transform,
            copy_sqs->quad_to_target_transform);
  EXPECT_EQ(source_sqs->quad_layer_rect, copy_sqs->quad_layer_rect);
  EXPECT_EQ(source_sqs->visible_quad_layer_rect,
            copy_sqs->visible_quad_layer_rect);
  EXPECT_EQ(source_sqs->clip_rect, copy_sqs->clip_rect);
  EXPECT_EQ(source_sqs->opacity, copy_sqs->opacity);
  EXPECT_EQ(source_sqs->blend_mode, copy_sqs->blend_mode);
  EXPECT_EQ(source_sqs->sorting_context_id, copy_sqs->sorting_context_id);
  EXPECT_EQ(source_sqs->mask_filter_info, copy_sqs->mask_filter_info);
  EXPECT_EQ(source_sqs->is_fast_rounded_corner,
            copy_sqs->is_fast_rounded_corner);
}

void CompareDrawQuad(DrawQuad* quad, DrawQuad* copy) {
  EXPECT_EQ(quad->material, copy->material);
  EXPECT_EQ(quad->rect, copy->rect);
  EXPECT_EQ(quad->visible_rect, copy->visible_rect);
  EXPECT_EQ(quad->needs_blending, copy->needs_blending);
  CompareSharedQuadState(quad->shared_quad_state, copy->shared_quad_state);
}

#define CREATE_SHARED_STATE()                                              \
  auto render_pass = CompositorRenderPass::Create();                       \
  SharedQuadState* shared_state(CreateSharedQuadState(render_pass.get())); \
  SharedQuadState* copy_shared_state =                                     \
      render_pass->CreateAndAppendSharedQuadState();                       \
  *copy_shared_state = *shared_state;

#define QUAD_DATA                                               \
  gfx::Rect quad_rect(30, 40, 50, 60);                          \
  [[maybe_unused]] gfx::Rect quad_visible_rect(40, 50, 30, 20); \
  [[maybe_unused]] bool needs_blending = true;

#define SETUP_AND_COPY_QUAD_NEW(Type, quad)                              \
  DrawQuad* copy_new = render_pass->CopyFromAndAppendDrawQuad(quad_new); \
  CompareDrawQuad(quad_new, copy_new);                                   \
  [[maybe_unused]] const Type* copy_quad = Type::MaterialCast(copy_new);

#define SETUP_AND_COPY_QUAD_ALL(Type, quad)                              \
  DrawQuad* copy_all = render_pass->CopyFromAndAppendDrawQuad(quad_all); \
  CompareDrawQuad(quad_all, copy_all);                                   \
  copy_quad = Type::MaterialCast(copy_all);

#define SETUP_AND_COPY_QUAD_NEW_RP(Type, quad, a)                    \
  DrawQuad* copy_new =                                               \
      render_pass->CopyFromAndAppendRenderPassDrawQuad(quad_new, a); \
  CompareDrawQuad(quad_new, copy_new);                               \
  [[maybe_unused]] const Type* copy_quad = Type::MaterialCast(copy_new);

#define SETUP_AND_COPY_QUAD_ALL_RP(Type, quad, a)                    \
  DrawQuad* copy_all =                                               \
      render_pass->CopyFromAndAppendRenderPassDrawQuad(quad_all, a); \
  CompareDrawQuad(quad_all, copy_all);                               \
  const Type* copy_quad = Type::MaterialCast(copy_all);

#define CREATE_QUAD_ALL(Type, ...)                                         \
  Type* quad_all = render_pass->CreateAndAppendDrawQuad<Type>();           \
  {                                                                        \
    QUAD_DATA quad_all->SetAll(shared_state, quad_rect, quad_visible_rect, \
                               needs_blending, __VA_ARGS__);               \
  }                                                                        \
  SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_NEW(Type, ...)                                      \
  Type* quad_new = render_pass->CreateAndAppendDrawQuad<Type>();        \
  { QUAD_DATA quad_new->SetNew(shared_state, quad_rect, __VA_ARGS__); } \
  SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_ALL_RP(Type, a, b, c, d, e, f, g, h, i, j, k, copy_a)     \
  Type* quad_all = render_pass->CreateAndAppendDrawQuad<Type>();              \
  {                                                                           \
    QUAD_DATA quad_all->SetAll(shared_state, quad_rect, a, needs_blending, b, \
                               c, d, e, f, g, h, i, j, k);                    \
  }                                                                           \
  SETUP_AND_COPY_QUAD_ALL_RP(Type, quad_all, copy_a);

#define CREATE_QUAD_NEW_RP(Type, a, b, c, d, e, f, g, h, i, j, copy_a)       \
  Type* quad_new = render_pass->CreateAndAppendDrawQuad<Type>();             \
  {                                                                          \
    QUAD_DATA quad_new->SetNew(shared_state, quad_rect, a, b, c, d, e, f, g, \
                               h, i, j);                                     \
  }                                                                          \
  SETUP_AND_COPY_QUAD_NEW_RP(Type, quad_new, copy_a);

TEST(DrawQuadTest, CopyDebugBorderDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor4f color = {0.7, 0.0, 0.1, 0.9};
  int width = 99;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(DebugBorderDrawQuad, visible_rect, color, width);
  EXPECT_EQ(DrawQuad::Material::kDebugBorder, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(width, copy_quad->width);

  CREATE_QUAD_ALL(DebugBorderDrawQuad, color, width);
  EXPECT_EQ(DrawQuad::Material::kDebugBorder, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(width, copy_quad->width);
}

TEST(DrawQuadTest, CopyRenderPassDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  CompositorRenderPassId render_pass_id{61};
  ResourceId mask_resource_id(78);
  gfx::RectF mask_uv_rect(0, 0, 33.f, 19.f);
  gfx::Size mask_texture_size(128, 134);
  gfx::Vector2dF filters_scale(1.0f, 1.0f);
  gfx::PointF filters_origin;
  gfx::RectF tex_coord_rect(1, 1, 255, 254);
  bool force_anti_aliasing_off = false;
  float backdrop_filter_quality = 1.0f;
  bool intersects_damage_under = false;

  CompositorRenderPassId copied_render_pass_id{235};
  CREATE_SHARED_STATE();

  CREATE_QUAD_ALL_RP(CompositorRenderPassDrawQuad, visible_rect, render_pass_id,
                     mask_resource_id, mask_uv_rect, mask_texture_size,
                     filters_scale, filters_origin, tex_coord_rect,
                     force_anti_aliasing_off, backdrop_filter_quality,
                     intersects_damage_under, copied_render_pass_id);
  EXPECT_EQ(DrawQuad::Material::kCompositorRenderPass, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(copied_render_pass_id, copy_quad->render_pass_id);
  EXPECT_EQ(mask_resource_id, copy_quad->mask_resource_id());
  EXPECT_EQ(mask_uv_rect.ToString(), copy_quad->mask_uv_rect.ToString());
  EXPECT_EQ(mask_texture_size.ToString(),
            copy_quad->mask_texture_size.ToString());
  EXPECT_EQ(filters_scale, copy_quad->filters_scale);
  EXPECT_EQ(filters_origin, copy_quad->filters_origin);
  EXPECT_EQ(tex_coord_rect.ToString(), copy_quad->tex_coord_rect.ToString());
  EXPECT_EQ(force_anti_aliasing_off, copy_quad->force_anti_aliasing_off);
  EXPECT_EQ(backdrop_filter_quality, copy_quad->backdrop_filter_quality);
  EXPECT_EQ(intersects_damage_under, copy_quad->intersects_damage_under);
}

TEST(DrawQuadTest, CopySolidColorDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor4f color = {0.28, 0.28, 0.28, 0.28};
  bool force_anti_aliasing_off = false;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(SolidColorDrawQuad, visible_rect, color,
                  force_anti_aliasing_off);
  EXPECT_EQ(DrawQuad::Material::kSolidColor, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(force_anti_aliasing_off, copy_quad->force_anti_aliasing_off);

  CREATE_QUAD_ALL(SolidColorDrawQuad, color, force_anti_aliasing_off);
  EXPECT_EQ(DrawQuad::Material::kSolidColor, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(force_anti_aliasing_off, copy_quad->force_anti_aliasing_off);
}

TEST(DrawQuadTest, CopySurfaceDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SurfaceId primary_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(1234, base::UnguessableToken::Create()));
  SurfaceId fallback_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(5678, base::UnguessableToken::Create()));
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(SurfaceDrawQuad, visible_rect,
                  SurfaceRange(fallback_surface_id, primary_surface_id),
                  SkColors::kWhite, /*stretch_content_to_fill_bounds=*/true);
  EXPECT_EQ(DrawQuad::Material::kSurfaceContent, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(primary_surface_id, copy_quad->surface_range.end());
  EXPECT_EQ(fallback_surface_id, *copy_quad->surface_range.start());
  EXPECT_TRUE(copy_quad->stretch_content_to_fill_bounds);

  CREATE_QUAD_ALL(SurfaceDrawQuad,
                  SurfaceRange(fallback_surface_id, primary_surface_id),
                  SkColors::kWhite, /*stretch_content_to_fill_bounds=*/false,
                  /*is_reflection=*/false, /*allow_merge=*/true);
  EXPECT_EQ(DrawQuad::Material::kSurfaceContent, copy_quad->material);
  EXPECT_EQ(primary_surface_id, copy_quad->surface_range.end());
  EXPECT_EQ(fallback_surface_id, *copy_quad->surface_range.start());
  EXPECT_FALSE(copy_quad->stretch_content_to_fill_bounds);
  EXPECT_FALSE(copy_quad->is_reflection);
}

TEST(DrawQuadTest, CopyTextureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool blending = true;
  ResourceId resource_id(82);
  gfx::Size resource_size_in_pixels = gfx::Size(40, 41);
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.5f, 224.f);
  gfx::PointF uv_bottom_right(51.5f, 260.f);
  bool y_flipped = true;
  bool nearest_neighbor = true;
  bool secure_output_only = true;
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kSoftwareProtected;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(TextureDrawQuad, visible_rect, blending, resource_id,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SkColors::kTransparent, y_flipped, nearest_neighbor,
                  secure_output_only, protected_video_type);
  EXPECT_EQ(DrawQuad::Material::kTextureContent, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(blending, copy_quad->needs_blending);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(premultiplied_alpha, copy_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);
  EXPECT_EQ(y_flipped, copy_quad->y_flipped);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
  EXPECT_EQ(secure_output_only, copy_quad->secure_output_only);
  EXPECT_EQ(protected_video_type, copy_quad->protected_video_type);
  EXPECT_FALSE(copy_quad->is_stream_video);

  CREATE_QUAD_ALL(TextureDrawQuad, resource_id, resource_size_in_pixels,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SkColors::kTransparent, y_flipped, nearest_neighbor,
                  secure_output_only, protected_video_type);
  EXPECT_EQ(DrawQuad::Material::kTextureContent, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels, copy_quad->resource_size_in_pixels());
  EXPECT_EQ(premultiplied_alpha, copy_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);
  EXPECT_EQ(y_flipped, copy_quad->y_flipped);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
  EXPECT_EQ(secure_output_only, copy_quad->secure_output_only);
  EXPECT_EQ(protected_video_type, copy_quad->protected_video_type);
  EXPECT_FALSE(copy_quad->is_stream_video);
}

TEST(DrawQuadTest, CopyTileDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool blending = true;
  ResourceId resource_id(104);
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool contents_premultiplied = true;
  bool nearest_neighbor = true;
  bool force_anti_aliasing_off = false;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(TileDrawQuad, visible_rect, blending, resource_id,
                  tex_coord_rect, texture_size, contents_premultiplied,
                  nearest_neighbor, force_anti_aliasing_off);
  EXPECT_EQ(DrawQuad::Material::kTiledContent, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(blending, copy_quad->needs_blending);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);

  CREATE_QUAD_ALL(TileDrawQuad, resource_id, tex_coord_rect, texture_size,
                  contents_premultiplied, nearest_neighbor,
                  force_anti_aliasing_off);
  EXPECT_EQ(DrawQuad::Material::kTiledContent, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
}

TEST(DrawQuadTest, CopyVideoHoleDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  base::UnguessableToken overlay_plane_id = base::UnguessableToken::Create();
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(VideoHoleDrawQuad, visible_rect, overlay_plane_id);
  EXPECT_EQ(DrawQuad::Material::kVideoHole, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(overlay_plane_id, copy_quad->overlay_plane_id);

  CREATE_QUAD_ALL(VideoHoleDrawQuad, overlay_plane_id);
  EXPECT_EQ(DrawQuad::Material::kVideoHole, copy_quad->material);
  EXPECT_EQ(overlay_plane_id, copy_quad->overlay_plane_id);
}

TEST(DrawQuadTest, CopyPictureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool blending = true;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool nearest_neighbor = true;
  gfx::Rect content_rect(30, 40, 20, 30);
  float contents_scale = 3.141592f;
  auto display_item_list =
      cc::FakeRasterSource::CreateEmpty(gfx::Size(100, 100))
          ->GetDisplayItemList();
  cc::ScrollOffsetMap raster_inducing_scroll_offsets = {
      {cc::ElementId(123), gfx::PointF(456.f, 789.f)}};
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(PictureDrawQuad, visible_rect, blending, tex_coord_rect,
                  texture_size, nearest_neighbor, content_rect, contents_scale,
                  {}, display_item_list, raster_inducing_scroll_offsets);
  EXPECT_EQ(DrawQuad::Material::kPictureContent, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(blending, copy_quad->needs_blending);
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
  EXPECT_EQ(content_rect, copy_quad->content_rect);
  EXPECT_EQ(contents_scale, copy_quad->contents_scale);
  EXPECT_EQ(display_item_list, copy_quad->display_item_list);
  EXPECT_EQ(raster_inducing_scroll_offsets,
            copy_quad->raster_inducing_scroll_offsets);
}

class DrawQuadIteratorTest : public testing::Test {
 protected:
  int IterateAndCount(DrawQuad* quad) {
    num_resources_ = 0;
    for (ResourceId& resource_id : quad->resources) {
      ++num_resources_;
      resource_id = NextId(resource_id);
    }
    return num_resources_;
  }

 private:
  int num_resources_;
};

TEST_F(DrawQuadIteratorTest, DebugBorderDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor4f color = {0.7, 0.0, 0.1, 0.9};
  int width = 99;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(DebugBorderDrawQuad, visible_rect, color, width);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST_F(DrawQuadIteratorTest, CompositorRenderPassDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  CompositorRenderPassId render_pass_id{61};
  ResourceId mask_resource_id(78);
  gfx::RectF mask_uv_rect(0.f, 0.f, 33.f, 19.f);
  gfx::Size mask_texture_size(128, 134);
  gfx::Vector2dF filters_scale(2.f, 3.f);
  gfx::PointF filters_origin(0.f, 0.f);
  gfx::RectF tex_coord_rect(1.f, 1.f, 33.f, 19.f);
  bool force_anti_aliasing_off = false;
  float backdrop_filter_quality = 1.0f;
  CompositorRenderPassId copied_render_pass_id{235};

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW_RP(CompositorRenderPassDrawQuad, visible_rect, render_pass_id,
                     mask_resource_id, mask_uv_rect, mask_texture_size,
                     filters_scale, filters_origin, tex_coord_rect,
                     force_anti_aliasing_off, backdrop_filter_quality,
                     copied_render_pass_id);
  EXPECT_EQ(mask_resource_id, quad_new->mask_resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(NextId(mask_resource_id), quad_new->mask_resource_id());

  ResourceId new_mask_resource_id = kInvalidResourceId;
  gfx::Rect quad_rect(30, 40, 50, 60);
  quad_new->SetNew(shared_state, quad_rect, visible_rect, render_pass_id,
                   new_mask_resource_id, mask_uv_rect, mask_texture_size,
                   filters_scale, filters_origin, tex_coord_rect,
                   force_anti_aliasing_off, backdrop_filter_quality);
  EXPECT_EQ(0, IterateAndCount(quad_new));
  EXPECT_EQ(kInvalidResourceId, quad_new->mask_resource_id());
}

TEST_F(DrawQuadIteratorTest, SolidColorDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor4f color = {0.28, 0.28, 0.28, 0.28};
  bool force_anti_aliasing_off = false;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(SolidColorDrawQuad, visible_rect, color,
                  force_anti_aliasing_off);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST_F(DrawQuadIteratorTest, SurfaceDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SurfaceId surface_id(kArbitraryFrameSinkId,
                       LocalSurfaceId(4321, base::UnguessableToken::Create()));

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(SurfaceDrawQuad, visible_rect,
                  SurfaceRange(std::nullopt, surface_id), SkColors::kWhite,
                  /*stretch_content_to_fill_bounds=*/false);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST_F(DrawQuadIteratorTest, TextureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  ResourceId resource_id(82);
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.5f, 224.f);
  gfx::PointF uv_bottom_right(51.5f, 260.f);
  bool y_flipped = true;
  bool nearest_neighbor = true;
  bool secure_output_only = true;
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(TextureDrawQuad, visible_rect, needs_blending, resource_id,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SkColors::kTransparent, y_flipped, nearest_neighbor,
                  secure_output_only, protected_video_type);
  EXPECT_EQ(resource_id, quad_new->resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(NextId(resource_id), quad_new->resource_id());
}

TEST_F(DrawQuadIteratorTest, TileDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  ResourceId resource_id(104);
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool contents_premultiplied = true;
  bool nearest_neighbor = true;
  bool force_anti_aliasing_off = false;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(TileDrawQuad, visible_rect, needs_blending, resource_id,
                  tex_coord_rect, texture_size, contents_premultiplied,
                  nearest_neighbor, force_anti_aliasing_off);
  EXPECT_EQ(resource_id, quad_new->resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(NextId(resource_id), quad_new->resource_id());
}

TEST_F(DrawQuadIteratorTest, VideoHoleDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  base::UnguessableToken overlay_plane_id = base::UnguessableToken::Create();

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(VideoHoleDrawQuad, visible_rect, overlay_plane_id);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST(DrawQuadTest, LargestQuadType) {
  size_t largest = 0;

  for (int i = 0; i <= static_cast<int>(DrawQuad::Material::kMaxValue); ++i) {
    switch (static_cast<DrawQuad::Material>(i)) {
      case DrawQuad::Material::kAggregatedRenderPass:
        largest = std::max(largest, sizeof(AggregatedRenderPassDrawQuad));
        break;
      case DrawQuad::Material::kDebugBorder:
        largest = std::max(largest, sizeof(DebugBorderDrawQuad));
        break;
      case DrawQuad::Material::kPictureContent:
        largest = std::max(largest, sizeof(PictureDrawQuad));
        break;
      case DrawQuad::Material::kTextureContent:
        largest = std::max(largest, sizeof(TextureDrawQuad));
        break;
      case DrawQuad::Material::kCompositorRenderPass:
        largest = std::max(largest, sizeof(CompositorRenderPassDrawQuad));
        break;
      case DrawQuad::Material::kSolidColor:
        largest = std::max(largest, sizeof(SolidColorDrawQuad));
        break;
      case DrawQuad::Material::kSurfaceContent:
        largest = std::max(largest, sizeof(SurfaceDrawQuad));
        break;
      case DrawQuad::Material::kTiledContent:
        largest = std::max(largest, sizeof(TileDrawQuad));
        break;
      case DrawQuad::Material::kVideoHole:
        largest = std::max(largest, sizeof(VideoHoleDrawQuad));
        break;
      case DrawQuad::Material::kSharedElement:
        largest = std::max(largest, sizeof(SharedElementDrawQuad));
        break;
      case DrawQuad::Material::kInvalid:
        break;
    }
  }
  EXPECT_EQ(LargestDrawQuadSize(), largest);

  if (!HasFailure())
    return;

  // On failure, output the size of all quads for debugging.
  LOG(ERROR) << "largest " << largest;
  LOG(ERROR) << "kLargestDrawQuad " << LargestDrawQuadSize();
  for (int i = 0; i <= static_cast<int>(DrawQuad::Material::kMaxValue); ++i) {
    switch (static_cast<DrawQuad::Material>(i)) {
      case DrawQuad::Material::kAggregatedRenderPass:
        LOG(ERROR) << "AggregatedRenderPass " << sizeof(AggregatedRenderPass);
        break;
      case DrawQuad::Material::kDebugBorder:
        LOG(ERROR) << "DebugBorderDrawQuad " << sizeof(DebugBorderDrawQuad);
        break;
      case DrawQuad::Material::kPictureContent:
        LOG(ERROR) << "PictureDrawQuad " << sizeof(PictureDrawQuad);
        break;
      case DrawQuad::Material::kTextureContent:
        LOG(ERROR) << "TextureDrawQuad " << sizeof(TextureDrawQuad);
        break;
      case DrawQuad::Material::kCompositorRenderPass:
        LOG(ERROR) << "CompositorRenderPassDrawQuad "
                   << sizeof(CompositorRenderPassDrawQuad);
        break;
      case DrawQuad::Material::kSolidColor:
        LOG(ERROR) << "SolidColorDrawQuad " << sizeof(SolidColorDrawQuad);
        break;
      case DrawQuad::Material::kSurfaceContent:
        LOG(ERROR) << "SurfaceDrawQuad " << sizeof(SurfaceDrawQuad);
        break;
      case DrawQuad::Material::kTiledContent:
        LOG(ERROR) << "TileDrawQuad " << sizeof(TileDrawQuad);
        break;
      case DrawQuad::Material::kVideoHole:
        LOG(ERROR) << "VideoHoleDrawQuad " << sizeof(VideoHoleDrawQuad);
        break;
      case DrawQuad::Material::kSharedElement:
        LOG(ERROR) << "SharedElementDrawQuad " << sizeof(SharedElementDrawQuad);
        break;
      case DrawQuad::Material::kInvalid:
        break;
    }
  }
}

class TextureDrawQuadTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<RoundedDisplayMasksInfo, gfx::RectF, gfx::RectF>> {
 public:
  TextureDrawQuadTest()
      : mask_info_(std::get<0>(GetParam())),
        expected_origin_mask_bounds_(std::get<1>(GetParam())),
        expected_other_mask_bounds_(std::get<2>(GetParam())) {}

  TextureDrawQuadTest(const TextureDrawQuadTest&) = delete;
  TextureDrawQuadTest& operator=(const TextureDrawQuadTest&) = delete;

  ~TextureDrawQuadTest() override = default;

 protected:
  void AddQuadWithRoundedDisplayMasks(
      gfx::Rect quad_rect,
      bool is_overlay_candidate,
      const gfx::Transform& quad_to_target_transform,
      const RoundedDisplayMasksInfo& rounded_display_masks_info,
      AggregatedRenderPass* render_pass) {
    SharedQuadState* quad_state = render_pass->CreateAndAppendSharedQuadState();

    quad_state->SetAll(
        /*transform=*/quad_to_target_transform, quad_rect,
        /*visible_layer_rect=*/quad_rect,
        /*filter_info=*/gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt,
        /*are contents opaque=*/true,
        /*opacity_f=*/1.f,
        /*blend=*/SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);

    TextureDrawQuad* texture_quad =
        render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    texture_quad->SetNew(quad_state, quad_rect, quad_rect,
                         /*needs_blending=*/true, ResourceId{1},
                         /*premultiplied=*/true, gfx::PointF(), gfx::PointF(),
                         /*background=*/SkColors::kTransparent,
                         /*flipped=*/false,
                         /*nearest=*/false,
                         /*secure_output=*/false,
                         gfx::ProtectedVideoType::kClear);

    texture_quad->rounded_display_masks_info = rounded_display_masks_info;
  }

  RoundedDisplayMasksInfo mask_info_;
  gfx::RectF expected_origin_mask_bounds_;
  gfx::RectF expected_other_mask_bounds_;
};

TEST_P(TextureDrawQuadTest, CorrectRoundedDisplayMaskBounds) {
  constexpr auto kTestQuadRect = gfx::Rect(0, 0, 100, 100);

  AggregatedRenderPass render_pass;
  gfx::Transform identity;
  identity.MakeIdentity();

  AddQuadWithRoundedDisplayMasks(kTestQuadRect,
                                 /*is_overlay_candidate=*/true, identity,
                                 mask_info_, &render_pass);

  const auto mask_bounds =
      TextureDrawQuad::RoundedDisplayMasksInfo::GetRoundedDisplayMasksBounds(
          render_pass.quad_list.front());

  EXPECT_EQ(
      mask_bounds[RoundedDisplayMasksInfo::kOriginRoundedDisplayMaskIndex],
      expected_origin_mask_bounds_);
  EXPECT_EQ(mask_bounds[RoundedDisplayMasksInfo::kOtherRoundedDisplayMaskIndex],
            expected_other_mask_bounds_);
}

INSTANTIATE_TEST_SUITE_P(
    /*no_prefix*/,
    TextureDrawQuadTest,
    testing::Values(
        std::make_tuple(
            RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                /*origin_rounded_display_mask_radius=*/10,
                /*other_rounded_display_mask_radius=*/15,
                /*is_horizontally_positioned=*/true),
            /*expected_origin_mask_bounds=*/gfx::RectF(0, 0, 10, 10),
            /*expected_other_mask_bounds=*/gfx::RectF(85, 0, 15, 15)),
        std::make_tuple(
            RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                /*origin_rounded_display_mask_radius=*/10,
                /*other_rounded_display_mask_radius=*/15,
                /*is_horizontally_positioned=*/false),
            /*expected_origin_mask_bounds=*/gfx::RectF(0, 0, 10, 10),
            /*expected_other_mask_bounds=*/gfx::RectF(0, 85, 15, 15)),
        std::make_tuple(
            RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                /*origin_rounded_display_mask_radius=*/0,
                /*other_rounded_display_mask_radius=*/15,
                /*is_horizontally_positioned=*/false),
            /*expected_origin_mask_bounds=*/gfx::RectF(),
            /*expected_other_mask_bounds=*/gfx::RectF(0, 85, 15, 15)),
        std::make_tuple(
            RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                /*origin_rounded_display_mask_radius=*/10,
                /*other_rounded_display_mask_radius=*/0,
                /*is_horizontally_positioned=*/false),
            /*expected_origin_mask_bounds=*/gfx::RectF(0, 0, 10, 10),
            /*expected_other_mask_bounds=*/gfx::RectF(0, 100, 0, 0)),
        std::make_tuple(RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                            /*origin_rounded_display_mask_radius=*/0,
                            /*other_rounded_display_mask_radius=*/0,
                            /*is_horizontally_positioned=*/false),
                        /*expected_origin_mask_bounds=*/gfx::RectF(),
                        /*expected_other_mask_bounds=*/gfx::RectF())));

}  // namespace
}  // namespace viz
