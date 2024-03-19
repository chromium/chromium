/// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_NUDGE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_NUDGE_CONTROLLER_H_

#include "base/component_export.h"
#include "chromeos/ash/components/growth/campaigns_model.h"

namespace growth {

// Dictionary of supported nudge payload. For example:
// {
//   "title": "Nudge title",
//   "body": "Body text"
// }
using NudgePayload = base::Value::Dict;

// A class that manages growth campaigns nudge.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
    CampaignsNudgeController {
 public:
  CampaignsNudgeController();
  CampaignsNudgeController(const CampaignsNudgeController&) = delete;
  CampaignsNudgeController& operator=(const CampaignsNudgeController&) = delete;
  ~CampaignsNudgeController();

  void ShowNudge(const NudgePayload* nudge_payload);
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_NUDGE_CONTROLLER_H_
