// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_

#include "chromeos/ash/components/growth/campaigns_manager_client.h"
#include "chromeos/ash/components/growth/campaigns_model.h"

class PrefService;

namespace growth {

class CampaignsMatcher {
 public:
  CampaignsMatcher(CampaignsManagerClient* client, PrefService* local_state);
  CampaignsMatcher(const CampaignsMatcher&) = delete;
  CampaignsMatcher& operator=(const CampaignsMatcher&) = delete;
  ~CampaignsMatcher();

  // Prefs related targeting will only be enabled after this method is call
  // explicitly to set user prefs.
  void SetPrefs(PrefService* prefs);

  void SetCampaigns(const CampaignsPerSlot* proactiveCampaigns,
                    const CampaignsPerSlot* reactiveCampaigns);

  // Select the targeted campaign for the given `slot`. Returns nullptr if no
  // campaign found for the given `slot`.
  const Campaign* GetCampaignBySlot(Slot slot) const;

 private:
  bool MatchDemoModeTier(const DemoModeTargeting& targeting) const;
  bool MatchDemoModeAppVersion(const DemoModeTargeting& targeting) const;
  bool MatchRetailers(const base::Value::List* retailers) const;
  bool MaybeMatchDemoModeTargeting(const DemoModeTargeting& targeting) const;
  bool MatchMilestone(const DeviceTargeting& targeting) const;
  bool MatchDeviceTargeting(const DeviceTargeting& targeting) const;
  bool Matched(const Targetings* targetings) const;

  // Owned by CampaignsManager.
  raw_ptr<const CampaignsPerSlot, ExperimentalAsh> proactive_campaigns_ =
      nullptr;
  raw_ptr<const CampaignsPerSlot, ExperimentalAsh> reactive_campaigns_ =
      nullptr;
  raw_ptr<CampaignsManagerClient, ExperimentalAsh> client_ = nullptr;
  raw_ptr<PrefService, ExperimentalAsh> local_state_ = nullptr;
  raw_ptr<PrefService, ExperimentalAsh> prefs_ = nullptr;
};

}  // namespace growth
#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_
