// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_model.h"

namespace growth {
namespace {

inline constexpr char kReactiveCampaigns[] = "reactiveCampaigns";
inline constexpr char kProactiveCampaigns[] = "proactiveCampaigns";

inline constexpr char kTargetings[] = "targetings";

inline constexpr char kPayloadPathTemplate[] = "payload.%s";
inline constexpr char kDemoModePayloadPath[] = "demoModeApp";

}  // namespace

const CampaignsPerSlot* GetProactiveCampaigns(
    const CampaignsStore* campaigns_store) {
  return campaigns_store->FindDict(kProactiveCampaigns);
}

const CampaignsPerSlot* GetReactiveCampaigns(
    const CampaignsStore* campaigns_store) {
  return campaigns_store->FindDict(kReactiveCampaigns);
}

const Campaigns* GetCampaignsBySlot(const CampaignsPerSlot* campaigns_per_slot,
                                    Slot slot) {
  if (!campaigns_per_slot) {
    return nullptr;
  }
  return campaigns_per_slot->FindList(base::NumberToString(int(slot)));
}

const Targetings* GetTargetings(const Campaign* campaign) {
  return campaign->FindList(kTargetings);
}

const Payload* GetPayloadBySlot(const Campaign* campaign, Slot slot) {
  if (slot == Slot::kDemoModeApp) {
    return campaign->FindDict(
        base::StringPrintf(kPayloadPathTemplate, kDemoModePayloadPath));
  }

  return nullptr;
}

}  // namespace growth
