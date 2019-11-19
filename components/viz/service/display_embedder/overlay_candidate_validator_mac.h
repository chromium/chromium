// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT OverlayCandidateValidatorMac
    : public OverlayCandidateValidator {
 public:
  explicit OverlayCandidateValidatorMac(bool ca_layer_disabled);
  ~OverlayCandidateValidatorMac() override;

  // OverlayCandidateValidator implementation.
  bool AllowCALayerOverlays() const override;
  bool AllowDCLayerOverlays() const override;
  bool NeedsSurfaceOccludingDamageRect() const override;
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override;

 private:
  const bool ca_layer_disabled_;

  DISALLOW_COPY_AND_ASSIGN(OverlayCandidateValidatorMac);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_
