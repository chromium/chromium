// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_WIN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_WIN_H_

#include "base/macros.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// This is a simple overlay candidate validator that promotes everything
// possible to an overlay.
class VIZ_SERVICE_EXPORT OverlayCandidateValidatorWin
    : public OverlayCandidateValidator {
 public:
  OverlayCandidateValidatorWin();
  ~OverlayCandidateValidatorWin() override;

  // OverlayCandidateValidator implementation.
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override;
  bool AllowCALayerOverlays() const override;
  bool AllowDCLayerOverlays() const override;
  bool NeedsSurfaceOccludingDamageRect() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayCandidateValidatorWin);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_WIN_H_
