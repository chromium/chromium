// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/overlay_candidate_validator_surface_control.h"

#include "cc/base/math_util.h"
#include "components/viz/service/display/overlay_candidate_list.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/renderer_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gl/android/android_surface_control_compat.h"

namespace viz {
namespace {

gfx::RectF ClipFromOrigin(gfx::RectF input) {
  if (input.x() < 0.f) {
    input.set_width(input.width() + input.x());
    input.set_x(0.f);
  }

  if (input.y() < 0) {
    input.set_height(input.height() + input.y());
    input.set_y(0.f);
  }

  return input;
}

}  // namespace

OverlayCandidateValidatorSurfaceControl::
    OverlayCandidateValidatorSurfaceControl() = default;
OverlayCandidateValidatorSurfaceControl::
    ~OverlayCandidateValidatorSurfaceControl() = default;

void OverlayCandidateValidatorSurfaceControl::InitializeStrategies() {
  strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
      this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
}

bool OverlayCandidateValidatorSurfaceControl::AllowCALayerOverlays() const {
  return false;
}

bool OverlayCandidateValidatorSurfaceControl::AllowDCLayerOverlays() const {
  return false;
}

bool OverlayCandidateValidatorSurfaceControl::NeedsSurfaceOccludingDamageRect()
    const {
  return true;
}

void OverlayCandidateValidatorSurfaceControl::CheckOverlaySupport(
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* surfaces) {
  DCHECK(!surfaces->empty());

  for (auto& candidate : *surfaces) {
    if (!gl::SurfaceControl::SupportsColorSpace(candidate.color_space)) {
      candidate.overlay_handled = false;
      return;
    }

    if (candidate.transform != display_transform_) {
      candidate.overlay_handled = false;
      return;
    }
    candidate.transform = gfx::OVERLAY_TRANSFORM_NONE;

    gfx::RectF orig_display_rect = candidate.display_rect;
    gfx::RectF display_rect = orig_display_rect;
    if (candidate.is_clipped)
      display_rect.Intersect(gfx::RectF(candidate.clip_rect));
    // The framework doesn't support display rects positioned at a negative
    // offset.
    display_rect = ClipFromOrigin(display_rect);
    if (display_rect.IsEmpty()) {
      candidate.overlay_handled = false;
      return;
    }

    // The display rect above includes the |display_transform_| while the rects
    // sent to the platform API need to be in the logical screen space.
    const gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
        gfx::InvertOverlayTransform(display_transform_),
        gfx::SizeF(viewport_size_));
    display_inverse.TransformRect(&orig_display_rect);
    display_inverse.TransformRect(&display_rect);

    candidate.display_rect = gfx::RectF(gfx::ToEnclosingRect(display_rect));
    candidate.uv_rect = cc::MathUtil::ScaleRectProportional(
        candidate.uv_rect, orig_display_rect, candidate.display_rect);
    candidate.overlay_handled = true;
  }
}

void OverlayCandidateValidatorSurfaceControl::AdjustOutputSurfaceOverlay(
    PrimaryPlane* output_surface_plane) {
  DCHECK(output_surface_plane);
  DCHECK(
      gl::SurfaceControl::SupportsColorSpace(output_surface_plane->color_space))
      << "The main overlay must only use color space supported by the "
         "device";

  DCHECK_EQ(output_surface_plane->transform, gfx::OVERLAY_TRANSFORM_NONE);
  DCHECK(output_surface_plane->display_rect ==
         ClipFromOrigin(output_surface_plane->display_rect));

  output_surface_plane->transform = display_transform_;
  const gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
      gfx::InvertOverlayTransform(display_transform_),
      gfx::SizeF(viewport_size_));
  display_inverse.TransformRect(&output_surface_plane->display_rect);
  output_surface_plane->display_rect =
      gfx::RectF(gfx::ToEnclosingRect(output_surface_plane->display_rect));
}

void OverlayCandidateValidatorSurfaceControl::SetDisplayTransform(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

void OverlayCandidateValidatorSurfaceControl::SetViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

gfx::Rect
OverlayCandidateValidatorSurfaceControl::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& candidate) const {
  // Should only be called after ProcessForOverlays on handled candidates.
  DCHECK(candidate.overlay_handled);
  // When the overlay is handled by the validator, we transform its display rect
  // to the logical screen space (used by the ui when preparing the frame) that
  // the SurfaceControl expects it to be in. So in order to provide a damage
  // rect which maps to the OutputSurface's main plane, we need to undo that
  // transformation.
  // But only if the overlay is in handled state, since the modification above
  // is only applied when we mark the overlay as handled.
  gfx::Size viewport_size_pre_display_transform(viewport_size_.height(),
                                                viewport_size_.width());
  auto transform = gfx::OverlayTransformToTransform(
      display_transform_, gfx::SizeF(viewport_size_pre_display_transform));
  gfx::RectF transformed_rect(candidate.display_rect);
  transform.TransformRect(&transformed_rect);
  return gfx::ToEnclosedRect(transformed_rect);
}

}  // namespace viz
