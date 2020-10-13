// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_surface_control.h"

#include <memory>

#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"

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

OverlayProcessorSurfaceControl::OverlayProcessorSurfaceControl()
    : OverlayProcessorUsingStrategy() {
  strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
      this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
}

OverlayProcessorSurfaceControl::~OverlayProcessorSurfaceControl() {}

bool OverlayProcessorSurfaceControl::IsOverlaySupported() const {
  return true;
}

bool OverlayProcessorSurfaceControl::NeedsSurfaceDamageRectList() const {
  return true;
}

void OverlayProcessorSurfaceControl::CheckOverlaySupport(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates) {
  DCHECK(!candidates->empty());

  for (auto& candidate : *candidates) {
    if (!gfx::SurfaceControl::SupportsColorSpace(candidate.color_space)) {
      candidate.overlay_handled = false;
      return;
    }

    // Check if screen rotation matches.
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

void OverlayProcessorSurfaceControl::AdjustOutputSurfaceOverlay(
    base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  // For surface control, we should always have a valid |output_surface_plane|
  // here.
  DCHECK(output_surface_plane && output_surface_plane->has_value());

  OutputSurfaceOverlayPlane& plane = output_surface_plane->value();
  DCHECK(gfx::SurfaceControl::SupportsColorSpace(plane.color_space))
      << "The main overlay must only use color space supported by the "
         "device";

  DCHECK_EQ(plane.transform, gfx::OVERLAY_TRANSFORM_NONE);
  DCHECK(plane.display_rect == ClipFromOrigin(plane.display_rect));

  plane.transform = display_transform_;
  const gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
      gfx::InvertOverlayTransform(display_transform_),
      gfx::SizeF(viewport_size_));
  display_inverse.TransformRect(&plane.display_rect);
  plane.display_rect = gfx::RectF(gfx::ToEnclosingRect(plane.display_rect));

  // Call the base class implementation.
  OverlayProcessorUsingStrategy::AdjustOutputSurfaceOverlay(
      output_surface_plane);
}

gfx::Rect OverlayProcessorSurfaceControl::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& candidate) const {
  // Should only be called after ProcessForOverlays on handled candidates.
  DCHECK(candidate.overlay_handled);
  // We transform the candidate's display rect to the logical screen space (used
  // by the ui when preparing the frame) that the SurfaceControl expects it to
  // be in. So in order to provide a damage rect which maps to the
  // OutputSurface's main plane, we need to undo that transformation. But only
  // if the overlay is in handled state, since the modification above is only
  // applied when we mark the overlay as handled.
  gfx::Size viewport_size_pre_display_transform(viewport_size_.height(),
                                                viewport_size_.width());
  auto transform = gfx::OverlayTransformToTransform(
      display_transform_, gfx::SizeF(viewport_size_pre_display_transform));
  gfx::RectF transformed_rect(candidate.display_rect);
  transform.TransformRect(&transformed_rect);
  return gfx::ToEnclosedRect(transformed_rect);
}

void OverlayProcessorSurfaceControl::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

void OverlayProcessorSurfaceControl::SetViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

}  // namespace viz
