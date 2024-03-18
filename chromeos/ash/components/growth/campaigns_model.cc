// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_model.h"

#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/growth/growth_metrics.h"

namespace growth {
namespace {

inline constexpr char kTargetings[] = "targetings";

inline constexpr char kId[] = "id";
inline constexpr char kStudyId[] = "studyId";

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
inline constexpr char kFeatureAware[] = "isFeatureAwareDevice";

// Session Targeting paths.
inline constexpr char kSessionTargeting[] = "session";

// Scheduling Targeting paths.
inline constexpr char kSchedulingTargetings[] = "schedulings";
inline constexpr char kSchedulingStart[] = "start";
inline constexpr char kSchedulingEnd[] = "end";

// Opened App Targeting paths.
inline constexpr char kAppsOpenedTargetings[] = "appsOpened";
inline constexpr char kAppId[] = "appId";

// Experiment Tag Targeting paths.
inline constexpr char kExperimentTargetings[] = "experimentTags";

// Payloads
inline constexpr char kPayloadPathTemplate[] = "payload.%s";
inline constexpr char kDemoModePayloadPath[] = "demoModeApp";

// Actions
inline constexpr char kActionTypePath[] = "type";
inline constexpr char kActionParamsPath[] = "params";

// Anchor paths.
inline constexpr char kActiveAppWindowAnchorType[] =
    "activeAppWindowAnchorType";
inline constexpr char kShelfAppButtonId[] = "shelfAppButtonId";

}  // namespace

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

std::optional<int> GetCampaignId(const Campaign* campaign) {
  return campaign->FindInt(kId);
}

std::optional<int> GetStudyId(const Campaign* campaign) {
  return campaign->FindInt(kStudyId);
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

const std::optional<bool> TargetingBase::GetBoolCriteria(
    const char* path_suffix) const {
  return targeting_->FindBoolByDottedPath(GetCriteriaPath(path_suffix));
}

const std::optional<int> TargetingBase::GetIntCriteria(
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

const std::optional<bool> DemoModeTargeting::TargetCloudGamingDevice() const {
  return GetBoolCriteria(kDemoModeCloudGaming);
}

const std::optional<bool> DemoModeTargeting::TargetFeatureAwareDevice() const {
  return GetBoolCriteria(kDemoModeFeatureAware);
}

// Device Targeting.
DeviceTargeting::DeviceTargeting(const Targeting* targeting_dict)
    : TargetingBase(targeting_dict, kDeviceTargeting) {}

DeviceTargeting::~DeviceTargeting() = default;

const base::Value::List* DeviceTargeting::GetLocales() const {
  return GetListCriteria(kDeviceLocales);
}

const std::optional<int> DeviceTargeting::GetMinMilestone() const {
  return GetIntCriteria(kMinMilestone);
}

const std::optional<int> DeviceTargeting::GetMaxMilestone() const {
  return GetIntCriteria(kMaxMilestone);
}

const std::optional<bool> DeviceTargeting::GetFeatureAwareDevice() const {
  return GetBoolCriteria(kFeatureAware);
}

// Apps Targeting.
AppTargeting::AppTargeting(const base::Value::Dict* app_dict)
    : app_dict_(app_dict) {}

AppTargeting::~AppTargeting() = default;

const std::string* AppTargeting::GetAppId() const {
  return app_dict_->FindString(kAppId);
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
    // TODO(b/308440474): Empty scheduling targeting is a valid use case. Remove
    // the error recording for that case.
    LOG(ERROR) << "Invalid scheduling targetings";
    RecordCampaignsManagerError(
        CampaignsManagerError::kInvalidSchedulingTargeting);
    return schedulings;
  }

  for (auto& scheduling_dict : *scheduling_dicts) {
    if (!scheduling_dict.is_dict()) {
      // Ignore invalid scheduling.
      RecordCampaignsManagerError(CampaignsManagerError::kInvalidScheduling);
      continue;
    }
    schedulings.push_back(
        std::make_unique<SchedulingTargeting>(&scheduling_dict.GetDict()));
  }
  return schedulings;
}

const base::Value::List* SessionTargeting::GetExperimentTags() const {
  return GetListCriteria(kExperimentTargetings);
}

Action::Action(const base::Value::Dict* action_dict)
    : action_dict_(action_dict) {}

std::optional<growth::ActionType> Action::GetActionType() const {
  auto action_type_value = action_dict_->FindInt(kActionTypePath);
  if (!action_type_value) {
    return std::nullopt;
  }

  return static_cast<growth::ActionType>(action_type_value.value());
}

const base::Value::Dict* Action::GetParams() const {
  return action_dict_->FindDict(kActionParamsPath);
}

const std::vector<std::unique_ptr<AppTargeting>>
SessionTargeting::GetAppsOpened() const {
  std::vector<std::unique_ptr<AppTargeting>> app_targetings;

  auto* app_targeting_dicts = GetListCriteria(kAppsOpenedTargetings);
  if (!app_targeting_dicts) {
    return app_targetings;
  }

  for (auto& app_targeting_dict : *app_targeting_dicts) {
    if (!app_targeting_dict.is_dict()) {
      // TODO(b/329124927): Record error.
      continue;
    }
    app_targetings.push_back(
        std::make_unique<AppTargeting>(&app_targeting_dict.GetDict()));
  }

  return app_targetings;
}

// Anchor.
Anchor::Anchor(const Targeting* anchor_dict) : anchor_dict_(anchor_dict) {}

const std::optional<WindowAnchorType> Anchor::GetActiveAppWindowAnchorType()
    const {
  if (!anchor_dict_) {
    // No valid anchor dict.
    return std::nullopt;
  }

  const auto anchor_type = anchor_dict_->FindInt(kActiveAppWindowAnchorType);
  if (!anchor_type) {
    // Invalid anchor type.
    // TODO(b/329698643): Record invalid anchor type metric.
    return std::nullopt;
  }

  return static_cast<WindowAnchorType>(anchor_type.value());
}

const std::string* Anchor::GetShelfAppButtonId() const {
  if (!anchor_dict_) {
    // No valid anchor dict.
    return nullptr;
  }

  return anchor_dict_->FindString(kShelfAppButtonId);
}

}  // namespace growth
