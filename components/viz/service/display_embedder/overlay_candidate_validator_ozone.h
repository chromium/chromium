// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace viz {

class VIZ_SERVICE_EXPORT OverlayCandidateValidatorOzone
    : public OverlayCandidateValidator {
 public:
  OverlayCandidateValidatorOzone(
      std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
      std::vector<OverlayStrategy> available_strategies);
  ~OverlayCandidateValidatorOzone() override;

  // OverlayCandidateValidator implementation.
  void InitializeStrategies() override;
  bool AllowCALayerOverlays() const override;
  bool AllowDCLayerOverlays() const override;
  bool NeedsSurfaceOccludingDamageRect() const override;
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override;
  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates_;
  const std::vector<OverlayStrategy> available_strategies_;
  bool software_mirror_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(OverlayCandidateValidatorOzone);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_
