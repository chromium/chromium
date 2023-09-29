// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_model.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace growth {
namespace {

inline constexpr char kReactiveCampaigns[] = "reactiveCampaigns";
inline constexpr char kProactiveCampaigns[] = "proactiveCampaigns";

inline constexpr char kTargetings[] = "targetings";

// Demo Mode targeting paths.
inline constexpr char kDemoModeTargeting[] = "demoMode";
inline constexpr char kDemoModeStoreIds[] = "storeIds";
inline constexpr char kDemoModeRetailers[] = "retailers";
inline constexpr char kDemoModeCountries[] = "countries";
inline constexpr char kDemoModeFeatureAware[] =
    "capability.isFeatureAwareDevice";
inline constexpr char kDemoModeCloudGaming[] = "capability.isCloudGamingDevice";

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

// Targeting Base.
TargetingBase::TargetingBase(const Targeting& targeting_dict,
                             const char* targeting_path)
    : targeting_(targeting_dict), targeting_path_(targeting_path) {}

TargetingBase::~TargetingBase() = default;

bool TargetingBase::IsValid() const {
  return !!targeting_.FindDict(targeting_path_);
}

const base::Value::List* TargetingBase::GetListCriteria(
    const char* path_suffix) const {
  return targeting_.FindListByDottedPath(GetCriteriaPath(path_suffix));
}

const absl::optional<bool> TargetingBase::GetBoolCriteria(
    const char* path_suffix) const {
  return targeting_.FindBoolByDottedPath(GetCriteriaPath(path_suffix));
}

const std::string* TargetingBase::GetStringCriteria(
    const char* path_suffix) const {
  return targeting_.FindStringByDottedPath(GetCriteriaPath(path_suffix));
}

const std::string TargetingBase::GetCriteriaPath(
    const char* path_suffix) const {
  return base::StringPrintf("%s.%s", targeting_path_, path_suffix);
}

// Demo Mode Targeting.
DemoModeTargeting::DemoModeTargeting(const Targeting& targeting_dict)
    : TargetingBase(targeting_dict, kDemoModeTargeting) {}

DemoModeTargeting::~DemoModeTargeting() = default;

const base::Value::List* DemoModeTargeting::GetStoreIds() const {
  return GetListCriteria(kDemoModeStoreIds);
}

const base::Value::List* DemoModeTargeting::GetRetailers() const {
  return GetListCriteria(kDemoModeRetailers);
}

const base::Value::List* DemoModeTargeting::GetCountries() const {
  return GetListCriteria(kDemoModeCountries);
}

const absl::optional<bool> DemoModeTargeting::TargetCloudGamingDevice() const {
  return GetBoolCriteria(kDemoModeCloudGaming);
}

const absl::optional<bool> DemoModeTargeting::TargetFeatureAwareDevice() const {
  return GetBoolCriteria(kDemoModeFeatureAware);
}

}  // namespace growth
