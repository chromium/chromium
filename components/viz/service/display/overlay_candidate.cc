// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/video_types.h"

namespace viz {

namespace {
// Tolerance for considering axis vector elements to be zero.
const SkMScalar kEpsilon = std::numeric_limits<float>::epsilon();

const gfx::BufferFormat kOverlayFormats[] = {
    gfx::BufferFormat::RGBX_8888, gfx::BufferFormat::RGBA_8888,
    gfx::BufferFormat::BGRX_8888, gfx::BufferFormat::BGRA_8888,
    gfx::BufferFormat::BGR_565,   gfx::BufferFormat::YUV_420_BIPLANAR};

enum Axis { NONE, AXIS_POS_X, AXIS_NEG_X, AXIS_POS_Y, AXIS_NEG_Y };

Axis VectorToAxis(const gfx::Vector3dF& vec) {
  if (std::abs(vec.z()) > kEpsilon)
    return NONE;
  const bool x_zero = (std::abs(vec.x()) <= kEpsilon);
  const bool y_zero = (std::abs(vec.y()) <= kEpsilon);
  if (x_zero && !y_zero)
    return (vec.y() > 0) ? AXIS_POS_Y : AXIS_NEG_Y;
  else if (y_zero && !x_zero)
    return (vec.x() > 0) ? AXIS_POS_X : AXIS_NEG_X;
  else
    return NONE;
}

gfx::OverlayTransform GetOverlayTransform(const gfx::Transform& quad_transform,
                                          bool y_flipped) {
  if (!quad_transform.Preserves2dAxisAlignment()) {
    return gfx::OVERLAY_TRANSFORM_INVALID;
  }

  gfx::Vector3dF x_axis = cc::MathUtil::GetXAxis(quad_transform);
  gfx::Vector3dF y_axis = cc::MathUtil::GetYAxis(quad_transform);
  if (y_flipped) {
    y_axis.Scale(-1);
  }

  Axis x_to = VectorToAxis(x_axis);
  Axis y_to = VectorToAxis(y_axis);

  if (x_to == AXIS_POS_X && y_to == AXIS_POS_Y)
    return gfx::OVERLAY_TRANSFORM_NONE;
  else if (x_to == AXIS_NEG_X && y_to == AXIS_POS_Y)
    return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
  else if (x_to == AXIS_POS_X && y_to == AXIS_NEG_Y)
    return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
  else if (x_to == AXIS_NEG_Y && y_to == AXIS_POS_X)
    return gfx::OVERLAY_TRANSFORM_ROTATE_270;
  else if (x_to == AXIS_NEG_X && y_to == AXIS_NEG_Y)
    return gfx::OVERLAY_TRANSFORM_ROTATE_180;
  else if (x_to == AXIS_POS_Y && y_to == AXIS_NEG_X)
    return gfx::OVERLAY_TRANSFORM_ROTATE_90;
  else
    return gfx::OVERLAY_TRANSFORM_INVALID;
}

}  // namespace

OverlayCandidate::OverlayCandidate()
    : transform(gfx::OVERLAY_TRANSFORM_NONE),
      format(gfx::BufferFormat::RGBA_8888),
      uv_rect(0.f, 0.f, 1.f, 1.f),
      is_clipped(false),
      is_opaque(false),
      no_occluding_damage(false),
      resource_id(0),
#if defined(OS_ANDROID)
      is_backed_by_surface_texture(false),
      is_promotable_hint(false),
#endif
      plane_z_order(0),
      is_unoccluded(false),
      overlay_handled(false),
      gpu_fence_id(0) {
}

OverlayCandidate::OverlayCandidate(const OverlayCandidate& other) = default;

OverlayCandidate::~OverlayCandidate() = default;

// static
bool OverlayCandidate::FromDrawQuad(DisplayResourceProvider* resource_provider,
                                    const SkMatrix44& output_color_matrix,
                                    const DrawQuad* quad,
                                    OverlayCandidate* candidate) {
  // It is currently not possible to set a color conversion matrix on an HW
  // overlay plane.
  // TODO(https://crbug.com/792757): Remove this check once the bug is resolved.
  if (!output_color_matrix.isIdentity())
    return false;

  // We don't support an opacity value different than one for an overlay plane.
  if (quad->shared_quad_state->opacity != 1.f)
    return false;
  // We can't support overlays with rounded corner clipping.
  if (!quad->shared_quad_state->rounded_corner_bounds.IsEmpty())
    return false;
  // We support only kSrc (no blending) and kSrcOver (blending with premul).
  if (!(quad->shared_quad_state->blend_mode == SkBlendMode::kSrc ||
        quad->shared_quad_state->blend_mode == SkBlendMode::kSrcOver)) {
    return false;
  }

  switch (quad->material) {
    case DrawQuad::Material::kTextureContent:
      return FromTextureQuad(resource_provider,
                             TextureDrawQuad::MaterialCast(quad), candidate);
    case DrawQuad::Material::kTiledContent:
      return FromTileQuad(resource_provider, TileDrawQuad::MaterialCast(quad),
                          candidate);
    case DrawQuad::Material::kVideoHole:
      return FromVideoHoleQuad(
          resource_provider, VideoHoleDrawQuad::MaterialCast(quad), candidate);
    case DrawQuad::Material::kStreamVideoContent:
      return FromStreamVideoQuad(resource_provider,
                                 StreamVideoDrawQuad::MaterialCast(quad),
                                 candidate);
    default:
      break;
  }

  return false;
}

// static
bool OverlayCandidate::IsInvisibleQuad(const DrawQuad* quad) {
  float opacity = quad->shared_quad_state->opacity;
  if (opacity < std::numeric_limits<float>::epsilon())
    return true;
  if (quad->material != DrawQuad::Material::kSolidColor)
    return false;
  const SkColor color = SolidColorDrawQuad::MaterialCast(quad)->color;
  const float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;
  return quad->ShouldDrawWithBlending() &&
         alpha < std::numeric_limits<float>::epsilon();
}

// static
bool OverlayCandidate::IsOccluded(const OverlayCandidate& candidate,
                                  QuadList::ConstIterator quad_list_begin,
                                  QuadList::ConstIterator quad_list_end) {
  // Check that no visible quad overlaps the candidate.
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::RectF overlap_rect = cc::MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect));
    if (candidate.display_rect.Intersects(overlap_rect) &&
        !OverlayCandidate::IsInvisibleQuad(*overlap_iter)) {
      return true;
    }
  }
  return false;
}

// static
bool OverlayCandidate::RequiresOverlay(const DrawQuad* quad) {
  switch (quad->material) {
    case DrawQuad::Material::kTextureContent:
      return TextureDrawQuad::MaterialCast(quad)->protected_video_type ==
             gfx::ProtectedVideoType::kHardwareProtected;
    case DrawQuad::Material::kVideoHole:
      return true;
    case DrawQuad::Material::kYuvVideoContent:
      return YUVVideoDrawQuad::MaterialCast(quad)->protected_video_type ==
             gfx::ProtectedVideoType::kHardwareProtected;
    default:
      return false;
  }
}

// static
bool OverlayCandidate::IsOccludedByFilteredQuad(
    const OverlayCandidate& candidate,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end,
    const base::flat_map<RenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters) {
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    if (overlap_iter->material == DrawQuad::Material::kRenderPass) {
      gfx::RectF overlap_rect = cc::MathUtil::MapClippedRect(
          overlap_iter->shared_quad_state->quad_to_target_transform,
          gfx::RectF(overlap_iter->rect));
      const RenderPassDrawQuad* render_pass_draw_quad =
          RenderPassDrawQuad::MaterialCast(*overlap_iter);
      if (candidate.display_rect.Intersects(overlap_rect) &&
          render_pass_backdrop_filters.count(
              render_pass_draw_quad->render_pass_id)) {
        return true;
      }
    }
  }
  return false;
}

// static
bool OverlayCandidate::FromDrawQuadResource(
    DisplayResourceProvider* resource_provider,
    const DrawQuad* quad,
    ResourceId resource_id,
    bool y_flipped,
    OverlayCandidate* candidate) {
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return false;

  candidate->format = resource_provider->GetBufferFormat(resource_id);
  candidate->color_space = resource_provider->GetColorSpace(resource_id);
  if (!base::Contains(kOverlayFormats, candidate->format))
    return false;

  gfx::OverlayTransform overlay_transform = GetOverlayTransform(
      quad->shared_quad_state->quad_to_target_transform, y_flipped);
  if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  auto& transform = quad->shared_quad_state->quad_to_target_transform;
  candidate->display_rect = gfx::RectF(quad->rect);
  transform.TransformRect(&candidate->display_rect);

  candidate->clip_rect = quad->shared_quad_state->clip_rect;
  candidate->is_clipped = quad->shared_quad_state->is_clipped;
  candidate->is_opaque = !quad->ShouldDrawWithBlending();
  if (quad->shared_quad_state->occluding_damage_rect.has_value()) {
    candidate->no_occluding_damage =
        quad->shared_quad_state->occluding_damage_rect->IsEmpty();
  }

  candidate->resource_id = resource_id;
  candidate->transform = overlay_transform;

  return true;
}

// static
// For VideoHoleDrawQuad, only calculate geometry information
// and put it in the |candidate|.
bool OverlayCandidate::FromVideoHoleQuad(
    DisplayResourceProvider* resource_provider,
    const VideoHoleDrawQuad* quad,
    OverlayCandidate* candidate) {
  gfx::OverlayTransform overlay_transform = GetOverlayTransform(
      quad->shared_quad_state->quad_to_target_transform, false);
  if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  auto& transform = quad->shared_quad_state->quad_to_target_transform;
  candidate->display_rect = gfx::RectF(quad->rect);
  transform.TransformRect(&candidate->display_rect);
  candidate->transform = overlay_transform;
  if (quad->shared_quad_state->occluding_damage_rect.has_value()) {
    candidate->no_occluding_damage =
        quad->shared_quad_state->occluding_damage_rect->IsEmpty();
  }

  return true;
}

// static
bool OverlayCandidate::FromTextureQuad(
    DisplayResourceProvider* resource_provider,
    const TextureDrawQuad* quad,
    OverlayCandidate* candidate) {
  if (quad->background_color != SK_ColorTRANSPARENT &&
      (quad->background_color != SK_ColorBLACK ||
       quad->ShouldDrawWithBlending()))
    return false;
  if (!FromDrawQuadResource(resource_provider, quad, quad->resource_id(),
                            quad->y_flipped, candidate)) {
    return false;
  }
  candidate->resource_size_in_pixels = quad->resource_size_in_pixels();
  candidate->uv_rect = BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  return true;
}

// static
bool OverlayCandidate::FromTileQuad(DisplayResourceProvider* resource_provider,
                                    const TileDrawQuad* quad,
                                    OverlayCandidate* candidate) {
  if (!FromDrawQuadResource(resource_provider, quad, quad->resource_id(), false,
                            candidate)) {
    return false;
  }
  candidate->resource_size_in_pixels = quad->texture_size;
  candidate->uv_rect = quad->tex_coord_rect;
  return true;
}

// static
bool OverlayCandidate::FromStreamVideoQuad(
    DisplayResourceProvider* resource_provider,
    const StreamVideoDrawQuad* quad,
    OverlayCandidate* candidate) {
  if (!FromDrawQuadResource(resource_provider, quad, quad->resource_id(), false,
                            candidate)) {
    return false;
  }

  candidate->resource_id = quad->resource_id();
  candidate->resource_size_in_pixels = quad->resource_size_in_pixels();
  candidate->uv_rect = BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
#if defined(OS_ANDROID)
  candidate->is_backed_by_surface_texture =
      resource_provider->IsBackedBySurfaceTexture(quad->resource_id());
#endif
  return true;
}
}  // namespace viz
