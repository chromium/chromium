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
#include "components/viz/service/display/display_resource_provider.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"

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

// Apply transform |delta| to |in| and return the resulting transform,
// or OVERLAY_TRANSFORM_INVALID.
gfx::OverlayTransform ComposeTransforms(gfx::OverlayTransform delta,
                                        gfx::OverlayTransform in) {
  // There are 8 different possible transforms. We can characterize these
  // by looking at where the origin moves and the direction the horizontal goes.
  // (TL=top-left, BR=bottom-right, H=horizontal, V=vertical).
  // NONE: TL, H
  // FLIP_VERTICAL: BL, H
  // FLIP_HORIZONTAL: TR, H
  // ROTATE_90: TR, V
  // ROTATE_180: BR, H
  // ROTATE_270: BL, V
  // Missing transforms: TL, V & BR, V
  // Basic combinations:
  // Flip X & Y -> Rotate 180 (TL,H -> TR,H -> BR,H or TL,H -> BL,H -> BR,H)
  // Flip X or Y + Rotate 180 -> other flip (eg, TL,H -> TR,H -> BL,H)
  // Rotate + Rotate simply adds values.
  // Rotate 90/270 + flip is invalid because we can only have verticals with
  // the origin in TR or BL.
  if (delta == gfx::OVERLAY_TRANSFORM_NONE)
    return in;
  switch (in) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return delta;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
          return gfx::OVERLAY_TRANSFORM_NONE;
        case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
          return gfx::OVERLAY_TRANSFORM_ROTATE_180;
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
          return gfx::OVERLAY_TRANSFORM_NONE;
        case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
          return gfx::OVERLAY_TRANSFORM_ROTATE_180;
        case gfx::OVERLAY_TRANSFORM_ROTATE_90:
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
        case gfx::OVERLAY_TRANSFORM_ROTATE_270:
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_ROTATE_90:
          return gfx::OVERLAY_TRANSFORM_ROTATE_180;
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_ROTATE_270;
        case gfx::OVERLAY_TRANSFORM_ROTATE_270:
          return gfx::OVERLAY_TRANSFORM_NONE;
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
          return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
        case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
          return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
        case gfx::OVERLAY_TRANSFORM_ROTATE_90:
          return gfx::OVERLAY_TRANSFORM_ROTATE_270;
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_NONE;
        case gfx::OVERLAY_TRANSFORM_ROTATE_270:
          return gfx::OVERLAY_TRANSFORM_ROTATE_90;
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_ROTATE_90:
          return gfx::OVERLAY_TRANSFORM_NONE;
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_ROTATE_90;
        case gfx::OVERLAY_TRANSFORM_ROTATE_270:
          return gfx::OVERLAY_TRANSFORM_ROTATE_180;
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    default:
      return gfx::OVERLAY_TRANSFORM_INVALID;
  }
}

}  // namespace

OverlayCandidate::OverlayCandidate()
    : transform(gfx::OVERLAY_TRANSFORM_NONE),
      format(gfx::BufferFormat::RGBA_8888),
      uv_rect(0.f, 0.f, 1.f, 1.f),
      is_clipped(false),
      is_opaque(false),
      use_output_surface_for_resource(false),
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
  // We support only kSrc (no blending) and kSrcOver (blending with premul).
  if (!(quad->shared_quad_state->blend_mode == SkBlendMode::kSrc ||
        quad->shared_quad_state->blend_mode == SkBlendMode::kSrcOver)) {
    return false;
  }

  switch (quad->material) {
    case DrawQuad::TEXTURE_CONTENT:
      return FromTextureQuad(resource_provider,
                             TextureDrawQuad::MaterialCast(quad), candidate);
    case DrawQuad::TILED_CONTENT:
      return FromTileQuad(resource_provider, TileDrawQuad::MaterialCast(quad),
                          candidate);
    case DrawQuad::STREAM_VIDEO_CONTENT:
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
  if (quad->material != DrawQuad::SOLID_COLOR)
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
bool OverlayCandidate::IsOccludedByFilteredQuad(
    const OverlayCandidate& candidate,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end,
    const base::flat_map<RenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters) {
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    if (overlap_iter->material == DrawQuad::RENDER_PASS) {
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
  if (!base::ContainsValue(kOverlayFormats, candidate->format))
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

  candidate->resource_id = resource_id;
  candidate->transform = overlay_transform;

  return true;
}

// static
bool OverlayCandidate::FromTextureQuad(
    DisplayResourceProvider* resource_provider,
    const TextureDrawQuad* quad,
    OverlayCandidate* candidate) {
  if (quad->background_color != SK_ColorTRANSPARENT)
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
  if (!quad->matrix.IsScaleOrTranslation()) {
    // We cannot handle anything other than scaling & translation for texture
    // coordinates yet.
    return false;
  }
  candidate->resource_id = quad->resource_id();
  candidate->resource_size_in_pixels = quad->resource_size_in_pixels();
#if defined(OS_ANDROID)
  candidate->is_backed_by_surface_texture =
      resource_provider->IsBackedBySurfaceTexture(quad->resource_id());
#endif

  gfx::Point3F uv0 = gfx::Point3F(0, 0, 0);
  gfx::Point3F uv1 = gfx::Point3F(1, 1, 0);
  quad->matrix.TransformPoint(&uv0);
  quad->matrix.TransformPoint(&uv1);
  gfx::Vector3dF delta = uv1 - uv0;
  if (delta.x() < 0) {
    candidate->transform = ComposeTransforms(
        gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL, candidate->transform);
    float x0 = uv0.x();
    uv0.set_x(uv1.x());
    uv1.set_x(x0);
    delta.set_x(-delta.x());
  }

  if (delta.y() < 0) {
    // In this situation, uv0y < uv1y. Since we overlay inverted, a request
    // to invert the source texture means we can just output the texture
    // normally and it will be correct.
    candidate->uv_rect = gfx::RectF(uv0.x(), uv1.y(), delta.x(), -delta.y());
  } else {
    candidate->transform = ComposeTransforms(
        gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL, candidate->transform);
    candidate->uv_rect = gfx::RectF(uv0.x(), uv0.y(), delta.x(), delta.y());
  }
  return true;
}

OverlayCandidateList::OverlayCandidateList() = default;

OverlayCandidateList::OverlayCandidateList(const OverlayCandidateList& other) =
    default;

OverlayCandidateList::OverlayCandidateList(OverlayCandidateList&& other) =
    default;

OverlayCandidateList::~OverlayCandidateList() = default;

OverlayCandidateList& OverlayCandidateList::operator=(
    const OverlayCandidateList& other) = default;

OverlayCandidateList& OverlayCandidateList::operator=(
    OverlayCandidateList&& other) = default;

void OverlayCandidateList::AddPromotionHint(const OverlayCandidate& candidate) {
  promotion_hint_info_map_[candidate.resource_id] = candidate.display_rect;
}

}  // namespace viz
