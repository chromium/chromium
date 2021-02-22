// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/video_types.h"

namespace viz {

// There is a bug in |base::optional| which causes the 'value_or' function to
// capture parameters (even constexpr parameters) as a reference.
constexpr uint32_t OverlayCandidate::kInvalidDamageIndex;

namespace {

const gfx::BufferFormat kOverlayFormats[] = {
    gfx::BufferFormat::RGBX_8888, gfx::BufferFormat::RGBA_8888,
    gfx::BufferFormat::BGRX_8888, gfx::BufferFormat::BGRA_8888,
    gfx::BufferFormat::BGR_565,   gfx::BufferFormat::YUV_420_BIPLANAR,
    gfx::BufferFormat::P010};

enum Axis { NONE, AXIS_POS_X, AXIS_NEG_X, AXIS_POS_Y, AXIS_NEG_Y };

Axis VectorToAxis(const gfx::Vector3dF& vec) {
  if (!cc::MathUtil::IsWithinEpsilon(vec.z(), 0.f))
    return NONE;
  const bool x_zero = cc::MathUtil::IsWithinEpsilon(vec.x(), 0.f);
  const bool y_zero = cc::MathUtil::IsWithinEpsilon(vec.y(), 0.f);
  if (x_zero && !y_zero)
    return (vec.y() > 0.f) ? AXIS_POS_Y : AXIS_NEG_Y;
  else if (y_zero && !x_zero)
    return (vec.x() > 0.f) ? AXIS_POS_X : AXIS_NEG_X;
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
    y_axis.Scale(-1.f);
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

gfx::Rect GetDamageRect(const DrawQuad* quad,
                        SurfaceDamageRectList* surface_damage_rect_list) {
  const SharedQuadState* sqs = quad->shared_quad_state;
  auto& transform = sqs->quad_to_target_transform;
  gfx::RectF display_rect = gfx::RectF(quad->rect);
  transform.TransformRect(&display_rect);
  if (!sqs->overlay_damage_index.has_value()) {
    gfx::Rect display_rect_int = gfx::ToRoundedRect(display_rect);
    // This is a special case where an overlay candidate may have damage but it
    // does not have a damage index since it was not the only quad in the
    // original surface. Here the union of all |surface_damage_rect_list| will
    // be in effect the full damage for this display.
    auto full_display_damage = gfx::Rect();
    for (auto& each : *surface_damage_rect_list) {
      full_display_damage.Union(each);
    }

    // We limit the damage to the candidates quad rect in question.
    gfx::Rect intersection = display_rect_int;
    intersection.Intersect(full_display_damage);
    return intersection;
  }

  size_t overlay_damage_index = sqs->overlay_damage_index.value();
  // Invalid index.
  if (overlay_damage_index >= surface_damage_rect_list->size()) {
    DCHECK(false);
    return gfx::Rect();
  }

  return (*surface_damage_rect_list)[overlay_damage_index];
}

}  // namespace

OverlayCandidate::OverlayCandidate() = default;

OverlayCandidate::OverlayCandidate(const OverlayCandidate& other) = default;

OverlayCandidate::~OverlayCandidate() = default;

// static
bool OverlayCandidate::FromDrawQuad(
    DisplayResourceProvider* resource_provider,
    SurfaceDamageRectList* surface_damage_rect_list,
    const SkMatrix44& output_color_matrix,
    const DrawQuad* quad,
    const gfx::RectF& primary_rect,
    OverlayCandidate* candidate) {
  // It is currently not possible to set a color conversion matrix on an HW
  // overlay plane.
  // TODO(https://crbug.com/792757): Remove this check once the bug is resolved.
  if (!output_color_matrix.isIdentity())
    return false;

  const SharedQuadState* sqs = quad->shared_quad_state;

  // We don't support an opacity value different than one for an overlay plane.
  if (sqs->opacity != 1.f)
    return false;

  // We can't support overlays with mask filter.
  if (!sqs->mask_filter_info.IsEmpty())
    return false;
  // We support only kSrc (no blending) and kSrcOver (blending with premul).
  if (!(sqs->blend_mode == SkBlendMode::kSrc ||
        sqs->blend_mode == SkBlendMode::kSrcOver)) {
    return false;
  }

  candidate->requires_overlay = OverlayCandidate::RequiresOverlay(quad);
  candidate->overlay_damage_index =
      sqs->overlay_damage_index.value_or(kInvalidDamageIndex);
  candidate->assume_damaged = !sqs->no_damage;

  switch (quad->material) {
    case DrawQuad::Material::kTextureContent:
      return FromTextureQuad(resource_provider, surface_damage_rect_list,
                             TextureDrawQuad::MaterialCast(quad), primary_rect,
                             candidate);
    case DrawQuad::Material::kVideoHole:
      return FromVideoHoleQuad(resource_provider, surface_damage_rect_list,
                               VideoHoleDrawQuad::MaterialCast(quad),
                               candidate);
    case DrawQuad::Material::kStreamVideoContent:
      return FromStreamVideoQuad(resource_provider, surface_damage_rect_list,
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
  if (cc::MathUtil::IsWithinEpsilon(opacity, 0.f))
    return true;
  if (quad->material != DrawQuad::Material::kSolidColor)
    return false;
  const SkColor color = SolidColorDrawQuad::MaterialCast(quad)->color;
  const float alpha = (SkColorGetA(color) * (1.f / 255.f)) * opacity;
  return quad->ShouldDrawWithBlending() &&
         cc::MathUtil::IsWithinEpsilon(alpha, 0.f);
}

// static
bool OverlayCandidate::IsOccluded(const OverlayCandidate& candidate,
                                  QuadList::ConstIterator quad_list_begin,
                                  QuadList::ConstIterator quad_list_end) {
  // The rects are rounded as they're snapped by the compositor to pixel unless
  // it is AA'ed, in which case, it won't be overlaid.
  gfx::Rect display_rect = gfx::ToRoundedRect(candidate.display_rect);

  // Check that no visible quad overlaps the candidate.
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::Rect overlap_rect = gfx::ToRoundedRect(cc::MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect)));

    if (!OverlayCandidate::IsInvisibleQuad(*overlap_iter) &&
        display_rect.Intersects(overlap_rect)) {
      return true;
    }
  }
  return false;
}

// static
int OverlayCandidate::EstimateVisibleDamage(
    const DrawQuad* quad,
    SurfaceDamageRectList* surface_damage_rect_list,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end) {
  gfx::Rect quad_damage = GetDamageRect(quad, surface_damage_rect_list);
  int occluded_damage_estimate_total = 0;
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::Rect overlap_rect = gfx::ToRoundedRect(cc::MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect)));

    // Opaque quad that (partially) occludes this candidate.
    if (!OverlayCandidate::IsInvisibleQuad(*overlap_iter) &&
        !overlap_iter->ShouldDrawWithBlending()) {
      overlap_rect.Intersect(quad_damage);
      occluded_damage_estimate_total += overlap_rect.size().GetArea();
    }
  }
  // In the case of overlapping UI the |occluded_damage_estimate_total| may
  // exceed the |quad|'s damage rect that is in consideration. This is the
  // reason why this computation is an estimate and why we have the max clamping
  // below.
  return std::max(
      0, quad_damage.size().GetArea() - occluded_damage_estimate_total);
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
    const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters) {
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    if (overlap_iter->material == DrawQuad::Material::kAggregatedRenderPass) {
      gfx::RectF overlap_rect = cc::MathUtil::MapClippedRect(
          overlap_iter->shared_quad_state->quad_to_target_transform,
          gfx::RectF(overlap_iter->rect));
      const auto* render_pass_draw_quad =
          AggregatedRenderPassDrawQuad::MaterialCast(*overlap_iter);
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
    SurfaceDamageRectList* surface_damage_rect_list,
    const DrawQuad* quad,
    ResourceId resource_id,
    bool y_flipped,
    OverlayCandidate* candidate) {
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return false;
  if (quad->visible_rect.IsEmpty())
    return false;

  candidate->format = resource_provider->GetBufferFormat(resource_id);
  candidate->color_space = resource_provider->GetColorSpace(resource_id);
  if (!base::Contains(kOverlayFormats, candidate->format))
    return false;

  const SharedQuadState* sqs = quad->shared_quad_state;
  gfx::OverlayTransform overlay_transform =
      GetOverlayTransform(sqs->quad_to_target_transform, y_flipped);
  if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  auto& transform = sqs->quad_to_target_transform;
  candidate->display_rect = gfx::RectF(quad->rect);
  transform.TransformRect(&candidate->display_rect);

  candidate->clip_rect = sqs->clip_rect;
  candidate->is_clipped = sqs->is_clipped;
  candidate->is_opaque = !quad->ShouldDrawWithBlending();
  // For underlays the function 'EstimateVisibleDamage()' is called to update
  // |damage_area_estimate| to more accurately reflect the actual visible
  // damage.
  candidate->damage_area_estimate =
      GetDamageRect(quad, surface_damage_rect_list).size().GetArea();
  candidate->resource_id = resource_id;
  candidate->transform = overlay_transform;
  candidate->mailbox = resource_provider->GetMailbox(resource_id);

  return true;
}

// static
// For VideoHoleDrawQuad, only calculate geometry information
// and put it in the |candidate|.
bool OverlayCandidate::FromVideoHoleQuad(
    DisplayResourceProvider* resource_provider,
    SurfaceDamageRectList* surface_damage_rect_list,
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
  // For underlays the function 'EstimateVisibleDamage()' is called to update
  // |damage_area_estimate| to more accurately reflect the actual visible
  // damage.
  candidate->damage_area_estimate =
      GetDamageRect(quad, surface_damage_rect_list).size().GetArea();
  return true;
}

// static
bool OverlayCandidate::FromTextureQuad(
    DisplayResourceProvider* resource_provider,
    SurfaceDamageRectList* surface_damage_rect_list,
    const TextureDrawQuad* quad,
    const gfx::RectF& primary_rect,
    OverlayCandidate* candidate) {
  if (quad->nearest_neighbor)
    return false;
  if (quad->background_color != SK_ColorTRANSPARENT &&
      (quad->background_color != SK_ColorBLACK ||
       quad->ShouldDrawWithBlending()))
    return false;

  if (!FromDrawQuadResource(resource_provider, surface_damage_rect_list, quad,
                            quad->resource_id(), quad->y_flipped, candidate)) {
    return false;
  }
  candidate->resource_size_in_pixels = quad->resource_size_in_pixels();
  candidate->uv_rect = BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  // Only handle clip rect for required overlays
  if (candidate->requires_overlay) {
    HandleClipAndSubsampling(candidate, primary_rect);
    candidate->hw_protected_validation_id = quad->hw_protected_validation_id;
  }
  return true;
}

// static
bool OverlayCandidate::FromStreamVideoQuad(
    DisplayResourceProvider* resource_provider,
    SurfaceDamageRectList* surface_damage_rect_list,
    const StreamVideoDrawQuad* quad,
    OverlayCandidate* candidate) {
  if (!FromDrawQuadResource(resource_provider, surface_damage_rect_list, quad,
                            quad->resource_id(), false, candidate)) {
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

// static
void OverlayCandidate::HandleClipAndSubsampling(
    OverlayCandidate* candidate,
    const gfx::RectF& primary_rect) {
  // The purpose of this is to enable overlays that are required (i.e. protected
  // content) to be able to be shown in all cases. This will allow them to pass
  // the clipping check and also the 2x alignment requirement for subsampling in
  // the Intel DRM driver. This should not be used in cases where the surface
  // will not always be promoted to an overlay as it will lead to shifting of
  // the content when it switches between composition and overlay.
  if (!candidate->is_clipped)
    return;

  // Make sure it's in a format we can deal with, we only support YUV and P010.
  if (candidate->format != gfx::BufferFormat::YUV_420_BIPLANAR &&
      candidate->format != gfx::BufferFormat::P010) {
    return;
  }
  // Clip the clip rect to the primary plane. An overlay will only be shown on
  // a single display, so we want to perform our calculations within the bounds
  // of that display.
  if (!primary_rect.IsEmpty())
    candidate->clip_rect.Intersect(gfx::ToNearestRect(primary_rect));

  // Calculate |uv_rect| of |clip_rect| in |display_rect|
  gfx::RectF uv_rect = cc::MathUtil::ScaleRectProportional(
      candidate->uv_rect, candidate->display_rect,
      gfx::RectF(candidate->clip_rect));

  // In case that |uv_rect| of candidate is not (0, 0, 1, 1)
  candidate->uv_rect.Intersect(uv_rect);

  // Update |display_rect| to avoid unexpected scaling and the candidate should
  // not be regarded as clippped after this.
  candidate->display_rect.Intersect(gfx::RectF(candidate->clip_rect));
  candidate->is_clipped = false;

  // Now correct |uv_rect| if required so that the source rect aligns on a pixel
  // boundary that is a multiple of the chroma subsampling.

  // Get the rect for the source coordinates.
  gfx::RectF src_rect = gfx::ScaleRect(
      candidate->uv_rect, candidate->resource_size_in_pixels.width(),
      candidate->resource_size_in_pixels.height());
  // Make it an integral multiple of the subsampling factor.
  auto subsample_round = [](float val) {
    constexpr int kSubsamplingFactor = 2;
    return (std::lround(val) / kSubsamplingFactor) * kSubsamplingFactor;
  };

  src_rect.set_x(subsample_round(src_rect.x()));
  src_rect.set_y(subsample_round(src_rect.y()));
  src_rect.set_width(subsample_round(src_rect.width()));
  src_rect.set_height(subsample_round(src_rect.height()));
  // Scale it back into UV space and set it in the candidate.
  candidate->uv_rect = gfx::ScaleRect(
      src_rect, 1.0f / candidate->resource_size_in_pixels.width(),
      1.0f / candidate->resource_size_in_pixels.height());
}

}  // namespace viz
