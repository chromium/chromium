// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/overlay_candidate_validator_mac.h"

#include <stddef.h>

namespace viz {

OverlayCandidateValidatorMac::OverlayCandidateValidatorMac(
    bool ca_layer_disabled)
    : ca_layer_disabled_(ca_layer_disabled) {}

OverlayCandidateValidatorMac::~OverlayCandidateValidatorMac() = default;

bool OverlayCandidateValidatorMac::AllowCALayerOverlays() const {
  return !ca_layer_disabled_;
}

bool OverlayCandidateValidatorMac::AllowDCLayerOverlays() const {
  return false;
}

bool OverlayCandidateValidatorMac::NeedsSurfaceOccludingDamageRect() const {
  return false;
}

void OverlayCandidateValidatorMac::CheckOverlaySupport(
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* surfaces) {}

}  // namespace viz
