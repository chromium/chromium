// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_

#include "chromeos/ash/components/growth/campaigns_model.h"

namespace growth {

class CampaignsMatcher {
 public:
  CampaignsMatcher();
  CampaignsMatcher(const CampaignsMatcher&) = delete;
  CampaignsMatcher& operator=(const CampaignsMatcher&) = delete;
  ~CampaignsMatcher();

  void SetCampaigns(const CampaignsPerSlot* proactiveCampaigns,
                    const CampaignsPerSlot* reactiveCampaigns);

  // Select the targeted campaign for the given `slot`. Returns nullptr if no
  // campaign found for the given `slot`.
  const Campaign* GetCampaignBySlot(Slot slot) const;

 private:
  // Owned by CampaignsManager.
  raw_ptr<const CampaignsPerSlot, ExperimentalAsh> proactive_campaigns_ =
      nullptr;
  raw_ptr<const CampaignsPerSlot, ExperimentalAsh> reactive_campaigns_ =
      nullptr;
};

}  // namespace growth
#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_
