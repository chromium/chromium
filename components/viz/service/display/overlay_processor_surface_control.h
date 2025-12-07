// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_

#include <optional>

#include "components/viz/service/display/overlay_processor_using_strategy.h"

namespace viz {

// This is an overlay processor implementation for Android SurfaceControl.
class VIZ_SERVICE_EXPORT OverlayProcessorSurfaceControl
    : public OverlayProcessorUsingStrategy {
 public:
  OverlayProcessorSurfaceControl();
  ~OverlayProcessorSurfaceControl() override;

  static std::optional<gfx::ColorSpace> GetOverrideColorSpace();

  bool IsOverlaySupported() const override;

  bool NeedsSurfaceDamageRectList() const override;

  // Override OverlayProcessorUsingStrategy.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  void SetViewportSize(const gfx::Size& size) override;
  void CheckOverlaySupportImpl(
      const std::optional<OverlayCandidate>& primary_plane,
      OverlayCandidateList* candidates) override;
  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const override;
  bool SupportsFlipRotateTransform() const override;

  void AdjustPrimaryPlaneForDisplayTransformForTesting(
      OverlayCandidate& primary_plane) const {
    AdjustPrimaryPlaneForDisplayTransform(primary_plane);
  }

 protected:
  void InsertPrimaryPlane(OverlayCandidate primary_plane,
                          OverlayCandidateList& candidates) override;

  virtual void AdjustPrimaryPlaneForDisplayTransform(
      OverlayCandidate& primary_plane) const;

 private:
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::Size viewport_size_;
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_
