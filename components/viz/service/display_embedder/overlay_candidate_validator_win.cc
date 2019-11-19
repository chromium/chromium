// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/overlay_candidate_validator_win.h"

#include "components/viz/service/display/overlay_processor.h"

namespace viz {

OverlayCandidateValidatorWin::OverlayCandidateValidatorWin() = default;

OverlayCandidateValidatorWin::~OverlayCandidateValidatorWin() = default;

void OverlayCandidateValidatorWin::CheckOverlaySupport(
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* candidates) {
  NOTIMPLEMENTED();
}

bool OverlayCandidateValidatorWin::AllowCALayerOverlays() const {
  return false;
}

bool OverlayCandidateValidatorWin::AllowDCLayerOverlays() const {
  return true;
}

bool OverlayCandidateValidatorWin::NeedsSurfaceOccludingDamageRect() const {
  return true;
}

}  // namespace viz
