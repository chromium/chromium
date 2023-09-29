// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_matcher.h"

#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "chromeos/ash/components/growth/campaigns_manager_client.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "components/prefs/pref_service.h"

namespace growth {
namespace {

bool MatchPref(const base::Value::List* criterias,
               base::StringPiece pref_path,
               const PrefService* pref_service) {
  if (!pref_service) {
    // TODO(b/299305911): This is unexpected. Add metrics to track this case.
    LOG(ERROR) << "Matching pref before pref service is available";
    return false;
  }

  if (!criterias) {
    // No related targeting found in campaign targeting, returns true.
    return true;
  }

  auto& value = pref_service->GetValue(pref_path);

  // String list targeting.
  if (criterias) {
    return Contains(*criterias, value);
  }

  return false;
}

}  // namespace

CampaignsMatcher::CampaignsMatcher(CampaignsManagerClient* client,
                                   PrefService* local_state)
    : client_(client), local_state_(local_state) {}
CampaignsMatcher::~CampaignsMatcher() = default;

void CampaignsMatcher::SetCampaigns(const CampaignsPerSlot* proactiveCampaigns,
                                    const CampaignsPerSlot* reactiveCampaigns) {
  proactive_campaigns_ = proactiveCampaigns;
  reactive_campaigns_ = reactiveCampaigns;
}

void CampaignsMatcher::SetPrefs(PrefService* prefs) {
  prefs_ = prefs;
}

const Campaign* CampaignsMatcher::GetCampaignBySlot(Slot slot) const {
  auto* targeted_campaigns = GetCampaignsBySlot(reactive_campaigns_, slot);
  if (!targeted_campaigns) {
    return nullptr;
  }

  for (auto& campaign_value : *targeted_campaigns) {
    const auto* campaign = campaign_value.GetIfDict();
    if (!campaign) {
      LOG(ERROR) << "Invalid campaign.";
      continue;
    }

    const auto* targetings = GetTargetings(campaign);

    if (Matched(targetings)) {
      return campaign;
    }
  }

  return nullptr;
}

bool CampaignsMatcher::MatchDemoModeTier(
    const DemoModeTargeting& targeting) const {
  const auto is_cloud_gaming = targeting.TargetCloudGamingDevice();
  if (is_cloud_gaming.has_value()) {
    if (is_cloud_gaming != client_->IsCloudGamingDevice()) {
      return false;
    }
  }

  const auto is_feature_aware_device = targeting.TargetFeatureAwareDevice();
  if (is_feature_aware_device.has_value()) {
    if (is_feature_aware_device != client_->IsFeatureAwareDevice()) {
      return false;
    }
  }
  return true;
}

bool CampaignsMatcher::MaybeMatchDemoModeTargeting(
    const DemoModeTargeting& targeting) const {
  if (!targeting.IsValid()) {
    // Campaigns matched if there is no demo mode targeting.
    return true;
  }

  if (!client_->IsDeviceInDemoMode()) {
    // Return early if it is not in demo mode while the campaign is targeting
    // demo mode.
    return false;
  }

  // TODO(b/298467438): Add demo mode app version targeting.

  if (!MatchDemoModeTier(targeting)) {
    return false;
  }

  return MatchPref(targeting.GetStoreIds(), ash::prefs::kDemoModeStoreId,
                   local_state_) &&
         MatchPref(targeting.GetRetailers(), ash::prefs::kDemoModeRetailerId,
                   local_state_) &&
         MatchPref(targeting.GetCountries(), ash::prefs::kDemoModeCountry,
                   local_state_);
}

bool CampaignsMatcher::Matched(const Targetings* targetings) const {
  // TODO(b/299305911): Add metrics to track matching latency.
  if (!targetings || targetings->empty()) {
    return true;
  }

  // TODO(b/299334282): Implement AND targeting operator when the list contains
  // more than one targeting.
  const auto* targeting = targetings->front().GetIfDict();
  if (!targeting) {
    // Targeting is invalid. Skip the current campaign.
    // TODO(b/299305911): Add metrics to track when a targeting is invalid.
    LOG(ERROR) << "Invalid targeting.";
    return false;
  }

  return MaybeMatchDemoModeTargeting(DemoModeTargeting(*targeting));
}

}  // namespace growth
