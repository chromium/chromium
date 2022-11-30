// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate.h"

#include "cc/base/math_util.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace viz {

// There is a bug in |base::optional| which causes the 'value_or' function to
// capture parameters (even constexpr parameters) as a reference.
constexpr uint32_t OverlayCandidate::kInvalidDamageIndex;
constexpr OverlayCandidate::TrackingId OverlayCandidate::kDefaultTrackingId;

OverlayCandidate::OverlayCandidate() = default;

OverlayCandidate::OverlayCandidate(const OverlayCandidate& other) = default;

OverlayCandidate::~OverlayCandidate() = default;

// static
bool OverlayCandidate::IsInvisibleQuad(const DrawQuad* quad) {
  float opacity = quad->shared_quad_state->opacity;
  if (cc::MathUtil::IsWithinEpsilon(opacity, 0.f))
    return true;
  if (quad->material != DrawQuad::Material::kSolidColor)
    return false;
  const float alpha =
      SolidColorDrawQuad::MaterialCast(quad)->color.fA * opacity;
  return quad->ShouldDrawWithBlending() &&
         cc::MathUtil::IsWithinEpsilon(alpha, 0.f);
}

// static
bool OverlayCandidate::IsOccluded(const OverlayCandidate& candidate,
                                  QuadList::ConstIterator quad_list_begin,
                                  QuadList::ConstIterator quad_list_end) {
  // The rects are rounded as they're snapped by the compositor to pixel unless
  // it is AA'ed, in which case, it won't be overlaid.
  gfx::RectF target_rect_f = candidate.display_rect;
  candidate.TransformRectToTargetSpace(target_rect_f);
  gfx::Rect target_rect = gfx::ToRoundedRect(target_rect_f);

  // Check that no visible quad overlaps the candidate.
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::Rect overlap_rect = gfx::ToRoundedRect(cc::MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect)));

    if (!OverlayCandidate::IsInvisibleQuad(*overlap_iter) &&
        target_rect.Intersects(overlap_rect)) {
      return true;
    }
  }
  return false;
}

// static
void OverlayCandidate::ApplyClip(OverlayCandidate& candidate,
                                 const gfx::RectF& clip_rect) {
  DCHECK(absl::holds_alternative<gfx::OverlayTransform>(candidate.transform));
  if (!clip_rect.Contains(candidate.display_rect)) {
    // Apply the buffer transform to the candidate's |uv_rect| so that it is
    // in the same orientation as |display_rect| when applying the clip.
    gfx::Transform buffer_transform = gfx::OverlayTransformToTransform(
        absl::get<gfx::OverlayTransform>(candidate.transform),
        gfx::SizeF(1, 1));
    candidate.uv_rect = buffer_transform.MapRect(candidate.uv_rect);

    gfx::RectF intersect_clip_display = clip_rect;
    intersect_clip_display.Intersect(candidate.display_rect);
    gfx::RectF uv_rect = cc::MathUtil::ScaleRectProportional(
        candidate.uv_rect, candidate.display_rect, intersect_clip_display);
    candidate.display_rect = intersect_clip_display;

    // Return |uv_rect| to buffer uv space.
    candidate.uv_rect =
        buffer_transform.InverseMapRect(uv_rect).value_or(uv_rect);
  }
}

void OverlayCandidate::TransformRectToTargetSpace(
    gfx::RectF& content_rect) const {
  if (absl::holds_alternative<gfx::Transform>(transform)) {
    content_rect = absl::get<gfx::Transform>(transform).MapRect(content_rect);
  }
}

}  // namespace viz
