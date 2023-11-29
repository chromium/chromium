// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_model.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/growth/growth_metrics.h"

namespace growth {
namespace {

inline constexpr char kReactiveCampaigns[] = "reactiveCampaigns";
inline constexpr char kProactiveCampaigns[] = "proactiveCampaigns";

inline constexpr char kTargetings[] = "targetings";

// Targetings.
// Demo Mode targeting paths.
inline constexpr char kDemoModeTargeting[] = "demoMode";
inline constexpr char kDemoModeStoreIds[] = "storeIds";
inline constexpr char kDemoModeRetailers[] = "retailers";
inline constexpr char kDemoModeCountries[] = "countries";
inline constexpr char kDemoModeFeatureAware[] =
    "capability.isFeatureAwareDevice";
inline constexpr char kDemoModeCloudGaming[] = "capability.isCloudGamingDevice";
inline constexpr char kMinDemoModeAppVersion[] = "appVersion.min";
inline constexpr char kMaxDemoModeAppVersion[] = "appVersion.max";

// Device Targeting paths.
inline constexpr char kDeviceTargeting[] = "device";
inline constexpr char kDeviceLocales[] = "locales";
inline constexpr char kMinMilestone[] = "milestone.min";
inline constexpr char kMaxMilestone[] = "milestone.max";

// Session Targeting paths.
inline constexpr char kSessionTargeting[] = "session";

// Scheduling Targeting paths.
inline constexpr char kSchedulingTargetings[] = "schedulings";
inline constexpr char kSchedulingStart[] = "start";
inline constexpr char kSchedulingEnd[] = "end";

// Payloads
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

// Return the payload for the given `slot`. Payload could be nullptr for running
// A/A testing. When payload is nullptr, fallback to the default behavior.
const Payload* GetPayloadBySlot(const Campaign* campaign, Slot slot) {
  if (slot == Slot::kDemoModeApp) {
    return campaign->FindDictByDottedPath(
        base::StringPrintf(kPayloadPathTemplate, kDemoModePayloadPath));
  }

  return nullptr;
}

// Targeting Base.
TargetingBase::TargetingBase(const Targeting* targeting_dict,
                             const char* targeting_path)
    : targeting_(targeting_dict), targeting_path_(targeting_path) {}

TargetingBase::~TargetingBase() = default;

bool TargetingBase::IsValid() const {
  return !!targeting_->FindDict(targeting_path_);
}

const base::Value::List* TargetingBase::GetListCriteria(
    const char* path_suffix) const {
  return targeting_->FindListByDottedPath(GetCriteriaPath(path_suffix));
}

const absl::optional<bool> TargetingBase::GetBoolCriteria(
    const char* path_suffix) const {
  return targeting_->FindBoolByDottedPath(GetCriteriaPath(path_suffix));
}

const absl::optional<int> TargetingBase::GetIntCriteria(
    const char* path_suffix) const {
  return targeting_->FindIntByDottedPath(GetCriteriaPath(path_suffix));
}

const std::string* TargetingBase::GetStringCriteria(
    const char* path_suffix) const {
  return targeting_->FindStringByDottedPath(GetCriteriaPath(path_suffix));
}

const std::string TargetingBase::GetCriteriaPath(
    const char* path_suffix) const {
  return base::StringPrintf("%s.%s", targeting_path_, path_suffix);
}

// Demo Mode Targeting.
DemoModeTargeting::DemoModeTargeting(const Targeting* targeting_dict)
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

const std::string* DemoModeTargeting::GetAppMinVersion() const {
  return GetStringCriteria(kMinDemoModeAppVersion);
}

const std::string* DemoModeTargeting::GetAppMaxVersion() const {
  return GetStringCriteria(kMaxDemoModeAppVersion);
}

const absl::optional<bool> DemoModeTargeting::TargetCloudGamingDevice() const {
  return GetBoolCriteria(kDemoModeCloudGaming);
}

const absl::optional<bool> DemoModeTargeting::TargetFeatureAwareDevice() const {
  return GetBoolCriteria(kDemoModeFeatureAware);
}

// Device Targeting.
DeviceTargeting::DeviceTargeting(const Targeting* targeting_dict)
    : TargetingBase(targeting_dict, kDeviceTargeting) {}

DeviceTargeting::~DeviceTargeting() = default;

const base::Value::List* DeviceTargeting::GetLocales() const {
  return GetListCriteria(kDeviceLocales);
}

const absl::optional<int> DeviceTargeting::GetMinMilestone() const {
  return GetIntCriteria(kMinMilestone);
}

const absl::optional<int> DeviceTargeting::GetMaxMilestone() const {
  return GetIntCriteria(kMaxMilestone);
}

// Scheduling Targeting.
SchedulingTargeting::SchedulingTargeting(
    const base::Value::Dict* scheduling_dict)
    : scheduling_dict_(scheduling_dict) {}

SchedulingTargeting::~SchedulingTargeting() = default;

const base::Time SchedulingTargeting::GetStartTime() const {
  auto start = scheduling_dict_->FindDouble(kSchedulingStart);
  if (start.has_value()) {
    return base::Time::FromSecondsSinceUnixEpoch(start.value());
  }

  return base::Time::Min();
}

const base::Time SchedulingTargeting::GetEndTime() const {
  auto end = scheduling_dict_->FindDouble(kSchedulingEnd);
  if (end.has_value()) {
    return base::Time::FromSecondsSinceUnixEpoch(end.value());
  }

  return base::Time::Max();
}

// Session Targeting.
SessionTargeting::SessionTargeting(const Targeting* targeting_dict)
    : TargetingBase(targeting_dict, kSessionTargeting) {}

SessionTargeting::~SessionTargeting() = default;

const std::vector<std::unique_ptr<SchedulingTargeting>>
SessionTargeting::GetSchedulings() const {
  std::vector<std::unique_ptr<SchedulingTargeting>> schedulings;

  auto* scheduling_dicts = GetListCriteria(kSchedulingTargetings);
  if (!scheduling_dicts) {
    LOG(ERROR) << "Invalid scheduling targetings";
    RecordCampaignsManagerError(
        CampaignsManagerError::kInvalidSchedulingTargeting);
    return schedulings;
  }

  for (auto& scheduling_dict : *scheduling_dicts) {
    if (!scheduling_dict.is_dict()) {
      RecordCampaignsManagerError(CampaignsManagerError::kInvalidScheduling);
      continue;
    }
    schedulings.push_back(
        std::make_unique<SchedulingTargeting>(&scheduling_dict.GetDict()));
  }
  return schedulings;
}

}  // namespace growth
