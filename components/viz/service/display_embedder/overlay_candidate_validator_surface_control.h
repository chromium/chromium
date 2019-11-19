// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_SURFACE_CONTROL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_SURFACE_CONTROL_H_

#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// Android OverlayCandidateValidator that uses surface control. Requires Android
// Q or higher.
class VIZ_SERVICE_EXPORT OverlayCandidateValidatorSurfaceControl
    : public OverlayCandidateValidator {
 public:
  OverlayCandidateValidatorSurfaceControl();
  ~OverlayCandidateValidatorSurfaceControl() override;

  // OverlayCandidateValidator implementation.
  void InitializeStrategies() override;
  bool AllowCALayerOverlays() const override;
  bool AllowDCLayerOverlays() const override;
  bool NeedsSurfaceOccludingDamageRect() const override;
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override;
  void AdjustOutputSurfaceOverlay(PrimaryPlane* output_surface_plane) override;
  void SetDisplayTransform(gfx::OverlayTransform transform) override;
  void SetViewportSize(const gfx::Size& viewport_size) override;
  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const override;

 private:
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::Size viewport_size_;

  DISALLOW_COPY_AND_ASSIGN(OverlayCandidateValidatorSurfaceControl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_SURFACE_CONTROL_H_
