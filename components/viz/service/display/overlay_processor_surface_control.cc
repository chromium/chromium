// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_surface_control.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "base/android/android_info.h"
#include "base/feature_list.h"
#include "cc/base/math_util.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace viz {
namespace {

BASE_FEATURE(kAndroidSurfaceControlSingleOnTOp,
             base::FEATURE_ENABLED_BY_DEFAULT);

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

OverlayProcessorSurfaceControl::OverlayProcessorSurfaceControl() {
  // Android webview never sets |frame_sequence_number_| for the overlay
  // processor. Android Chrome does set this variable because it does call draw.
  // However, it also may not update this variable when displaying an overlay.
  // Therefore, our damage tracking for overlays is incorrect and we must ignore
  // the thresholding of prioritization.

  // TODO(crbug.com/40236858): We should take issue into account when trying to
  // find a replacement for number-of-scanouts.
  prioritization_config_.changing_threshold = false;
  prioritization_config_.damage_rate_threshold = false;

  strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
      this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
  if (base::FeatureList::IsEnabled(kAndroidSurfaceControlSingleOnTOp)) {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    // Prefer underlay strategy because it is more mature on Android. So turn
    // off sorting and just attempt the strategies in insertion order.
    prioritization_config_.power_gain_sort = false;
  }
}

OverlayProcessorSurfaceControl::~OverlayProcessorSurfaceControl() = default;

bool OverlayProcessorSurfaceControl::IsOverlaySupported() const {
  return true;
}

bool OverlayProcessorSurfaceControl::NeedsSurfaceDamageRectList() const {
  return true;
}

void OverlayProcessorSurfaceControl::CheckOverlaySupportImpl(
    const std::optional<OverlayCandidate>& primary_plane,
    OverlayCandidateList* candidates) {
  DCHECK(!candidates->empty());

  for (auto& candidate : *candidates) {
    if (auto override_color_space = GetOverrideColorSpace()) {
      candidate.color_space = override_color_space.value();
      candidate.hdr_metadata = gfx::HDRMetadata();
    }

    // Check if the ColorSpace is supported
    if (!gfx::SurfaceControl::SupportsColorSpace(candidate.color_space)) {
      candidate.overlay_handled = false;
      return;
    }

    // Aggregator adds `display_transform_` to all quads, which is then added to
    // `candidate.transform` here. `display_transform_` only applies to content
    // on the main plane so it needs to be removed candidate it its own plane.
    gfx::OverlayTransform candidate_overlay_transform = OverlayTransformsConcat(
        std::get<gfx::OverlayTransform>(candidate.transform),
        InvertOverlayTransform(display_transform_));
    // Note the transform below using `candidate_overlay_transform` to compute
    // clipped and normalized `uv_rect` is only tested with NONE and
    // FLIP_VERTICAL.
    if (candidate_overlay_transform != gfx::OVERLAY_TRANSFORM_NONE &&
        candidate_overlay_transform != gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL) {
      candidate.overlay_handled = false;
      return;
    }
    candidate.transform = candidate_overlay_transform;

    gfx::RectF orig_display_rect = candidate.display_rect;
    gfx::RectF display_rect = orig_display_rect;
    if (candidate.clip_rect)
      display_rect.Intersect(gfx::RectF(*candidate.clip_rect));
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
    orig_display_rect = display_inverse.MapRect(orig_display_rect);
    display_rect = display_inverse.MapRect(display_rect);

    candidate.unclipped_display_rect = orig_display_rect;
    candidate.unclipped_uv_rect = candidate.uv_rect;

    candidate.display_rect = gfx::RectF(gfx::ToEnclosingRect(display_rect));

    // Transform `uv_rect` to display space, then clip, then transform back.
    candidate.uv_rect = gfx::OverlayTransformToTransform(
                            candidate_overlay_transform, gfx::SizeF(1, 1))
                            .MapRect(candidate.uv_rect);
    candidate.uv_rect = cc::MathUtil::ScaleRectProportional(
        candidate.uv_rect, orig_display_rect, candidate.display_rect);
    candidate.uv_rect =
        gfx::OverlayTransformToTransform(
            gfx::InvertOverlayTransform(candidate_overlay_transform),
            gfx::SizeF(1, 1))
            .MapRect(candidate.uv_rect);
    candidate.overlay_handled = true;
  }
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
  return transform.MapRect(gfx::ToEnclosingRect(candidate.display_rect));
}

bool OverlayProcessorSurfaceControl::SupportsFlipRotateTransform() const {
  return true;
}

void OverlayProcessorSurfaceControl::InsertPrimaryPlane(
    OverlayCandidate primary_plane,
    OverlayCandidateList& candidates) {
  AdjustPrimaryPlaneForDisplayTransform(primary_plane);

  // Android respects plane_z_order and order in the list shouldn't matter,
  // but it surfaces the bug when the planes are not hidden properly. As we
  // use only underlays, we should keep primary plane first so it would hide
  // planes that are not supposed to be visible.
  const auto insert_positon = candidates.begin();
  candidates.insert(insert_positon, std::move(primary_plane));
}

void OverlayProcessorSurfaceControl::AdjustPrimaryPlaneForDisplayTransform(
    OverlayCandidate& primary_plane) const {
  // Apply the display transform hint which will be non-identity if output
  // surface's `orientation_mode` is `kHardware` and the display's hardware
  // orientation does not match the OS's logical orientation. This will allow us
  // to draw into the primary plane in the orientation that allows the hardware
  // to make use of hardware overlays.
  DCHECK(gfx::SurfaceControl::SupportsColorSpace(primary_plane.color_space))
      << "The main overlay must only use color space supported by the device";
  DCHECK(
      std::holds_alternative<gfx::OverlayTransform>(primary_plane.transform));
  DCHECK_EQ(std::get<gfx::OverlayTransform>(primary_plane.transform),
            gfx::OVERLAY_TRANSFORM_NONE);
  DCHECK(primary_plane.display_rect ==
         ClipFromOrigin(primary_plane.display_rect));
  primary_plane.transform = display_transform_;
  const gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
      gfx::InvertOverlayTransform(display_transform_),
      gfx::SizeF(viewport_size_));
  primary_plane.display_rect =
      display_inverse.MapRect(primary_plane.display_rect);
  primary_plane.display_rect =
      gfx::RectF(gfx::ToEnclosingRect(primary_plane.display_rect));
}

void OverlayProcessorSurfaceControl::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

void OverlayProcessorSurfaceControl::SetViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

std::optional<gfx::ColorSpace>
OverlayProcessorSurfaceControl::GetOverrideColorSpace() {
  // historically, android media was hardcoding color space to srgb and it
  // wasn't possible to overlay with arbitrary colorspace on pre-S devices, so
  // we keep old behaviour there.
  static bool is_older_than_s = base::android::android_info::sdk_int() <
                                base::android::android_info::SDK_VERSION_S;
  if (is_older_than_s) {
    return gfx::ColorSpace::CreateSRGB();
  }

  return std::nullopt;
}

}  // namespace viz
