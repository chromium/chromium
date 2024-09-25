// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/growth/campaigns_model.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chromeos/ash/components/growth/action_performer.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/vector_icon_types.h"

namespace growth {
namespace {

inline constexpr char kTargetings[] = "targetings";

inline constexpr char kId[] = "id";
inline constexpr char kGroupId[] = "groupId";
inline constexpr char kStudyId[] = "studyId";
inline constexpr char kShouldRegisterTrialWithTriggerEventName[] =
    "registerTrialWithTriggerEventName";

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
inline constexpr char kApplicationLocales[] = "locales";
inline constexpr char kUserLocales[] = "userLocales";
inline constexpr char kIncludedCountries[] = "includedCountries";
inline constexpr char kExcludedCountries[] = "excludedCountries";
inline constexpr char kMinMilestone[] = "milestone.min";
inline constexpr char kMaxMilestone[] = "milestone.max";
inline constexpr char kMinVersion[] = "version.min";
inline constexpr char kMaxVersion[] = "version.max";
inline constexpr char kFeatureAware[] = "isFeatureAwareDevice";
inline constexpr char kRegisteredTime[] = "registeredTime";
inline constexpr char kDeviceAgeInHours[] = "deviceAgeInHours";

// Session Targeting paths.
inline constexpr char kSessionTargeting[] = "session";

// Experiment Tag Targeting paths.
inline constexpr char kPredefinedFeatureIndex[] = "predefinedFeatureIndex";
inline constexpr char kOneOffExpFeatureIndex[] = "oneOffExpFeatureIndex";
inline constexpr char kExperimentTargetings[] = "experimentTags";

// User Targeting paths.
inline constexpr char kMinorUser[] = "isMinorUser";
inline constexpr char kOwner[] = "isOwner";

// Events Targeting paths.
inline constexpr char kEventsTargetings[] = "events";
inline constexpr char kImpressionCap[] = "impressionCap";
inline constexpr char kDismissalCap[] = "dismissalCap";
inline constexpr char kGroupImpressionCap[] = "groupImpressionCap";
inline constexpr char kGroupDismissalCap[] = "groupDismissalCap";
inline constexpr char kEventsConditions[] = "conditions";
inline constexpr int kImpressionCapDefaultValue = 3;
inline constexpr int kDismissalCapDefaultValue = 1;

// Runtime Targeting paths.
inline constexpr char kRuntimeTargeting[] = "runtime";

// Trigger Targeting paths.
// Path `triggers` was used in M126 and has been deprecated since M127.
// Path `triggersList` was added for M127.
inline constexpr char kTriggerTargetings[] = "triggerList";
inline constexpr char kTriggerType[] = "triggerType";
inline constexpr char kTriggerEvents[] = "triggerEvents";

// User Preference Targeting path.
inline constexpr char kUserPrefTargetings[] = "userPrefs";

// Shelf Hotseat Targeting path.
inline constexpr char kHotseatAppIcon[] = "hotseat.appIcon";

// Scheduling Targeting paths.
inline constexpr char kSchedulingTargetings[] = "schedulings";
inline constexpr char kTimeWindowStart[] = "start";
inline constexpr char kTimeWindowEnd[] = "end";

// Number Range Targeting paths.
inline constexpr char kNumberRangeStart[] = "start";
inline constexpr char kNumberRangeEnd[] = "end";

// Opened App Targeting paths.
inline constexpr char kAppsOpenedTargetings[] = "appsOpened";
inline constexpr char kAppId[] = "appId";

// Active URL regexes path.
inline constexpr char kActiveUrlRegexes[] = "activeUrlRegexes";

// Payloads
inline constexpr char kPayloadPathTemplate[] = "payload.%s";
inline constexpr char kDemoModePayloadPath[] = "demoModeApp";
inline constexpr char kNudgePayloadPath[] = "nudge";
inline constexpr char kNotificationPayloadPath[] = "notification";
inline constexpr char kOobePerkDiscoveryPayloadPath[] = "oobePerkDiscovery";

// Actions
inline constexpr char kActionTypePath[] = "type";
inline constexpr char kActionParamsPath[] = "params";

// Anchor paths.
inline constexpr char kActiveAppWindowAnchorType[] =
    "activeAppWindowAnchorType";
inline constexpr char kShelfAppButtonId[] = "shelfAppButtonId";

// Image Model.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr char kImage[] = "image";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr int kIconSize = 60;
inline constexpr char kVectorIcon[] = "vectorIcon";

// Vector Icon
inline constexpr char kBuiltInVectorIcon[] = "builtInVectorIcon";

// Each feature will be used in one finch study.
// These features are reusable if a feature is not currently used.
// Entries should not be ordered as feature is selected by index defined in the
// campaign.
inline const base::Feature* kPredefinedFeaturesForExperimentTagTargeting[] = {
    &ash::features::kGrowthCampaignsExperiment1,
    &ash::features::kGrowthCampaignsExperiment2,
    &ash::features::kGrowthCampaignsExperiment3,
    &ash::features::kGrowthCampaignsExperiment4,
    &ash::features::kGrowthCampaignsExperiment5,
    &ash::features::kGrowthCampaignsExperiment6,
    &ash::features::kGrowthCampaignsExperiment7,
    &ash::features::kGrowthCampaignsExperiment8,
    &ash::features::kGrowthCampaignsExperiment9,
    &ash::features::kGrowthCampaignsExperiment10,
    &ash::features::kGrowthCampaignsExperiment11,
    &ash::features::kGrowthCampaignsExperiment12,
    &ash::features::kGrowthCampaignsExperiment13,
    &ash::features::kGrowthCampaignsExperiment14,
    &ash::features::kGrowthCampaignsExperiment15,
    &ash::features::kGrowthCampaignsExperiment16,
    &ash::features::kGrowthCampaignsExperiment17,
    &ash::features::kGrowthCampaignsExperiment18,
    &ash::features::kGrowthCampaignsExperiment19,
    &ash::features::kGrowthCampaignsExperiment20,
};

// List of one-off feature flags used for delivering finch params for
// study/groups that refer to more than one feature flags.
// Each feature will be used in one finch study.
// These features are not reusable. It is tied to the finch config of a
// particular experiment.
// Entries should not be ordered as feature is selected by index defined in the
// campaign.
const base::Feature* kOneOffFeaturesForExperimentTagTargeting[] = {
    &ash::features::kGrowthCampaignsExperimentG1Nudge,
    &ash::features::kGrowthCampaignsExperimentFileAppGamgee,
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Image
inline constexpr char kBuiltInImage[] = "builtInImage";

inline constexpr gfx::Size kBubbleIconSizeDip = gfx::Size(kIconSize, kIconSize);

std::optional<int> GetBuiltInImageResourceId(
    const std::optional<BuiltInImage>& image_model_type) {
  if (!image_model_type) {
    return std::nullopt;
  }

  switch (image_model_type.value()) {
    case BuiltInImage::kContainerApp:
      return IDR_GROWTH_FRAMEWORK_CONTAINER_APP_PNG;
    case BuiltInImage::kG1:
      return IDR_GROWTH_FRAMEWORK_G1_PNG;
    case BuiltInImage::kSparkRebuy:
      return IDR_GROWTH_FRAMEWORK_SPARK_REBUY_PNG;
    case BuiltInImage::kSpark1PApp:
      return IDR_GROWTH_FRAMEWORK_SPARK_1P_APP_PNG;
    case BuiltInImage::kSparkV2:
      return IDR_GROWTH_FRAMEWORK_SPARK_V2_PNG;
    case BuiltInImage::kG1Notification:
      return IDR_GROWTH_FRAMEWORK_G1_NOTIFICATION_PNG;
    case BuiltInImage::kMall:
      return IDR_GROWTH_FRAMEWORK_MALL_PNG;
  }
}

std::optional<BuiltInImage> GetBuiltInImageType(
    const base::Value::Dict* image_dict) {
  auto built_in_image_value = image_dict->FindInt(kBuiltInImage);
  if (!built_in_image_value) {
    return std::nullopt;
  }

  auto built_in_image = built_in_image_value.value();
  if (built_in_image < 0 ||
      built_in_image > static_cast<int>(BuiltInImage::kMaxValue)) {
    return std::nullopt;
  }

  return static_cast<BuiltInImage>(built_in_image);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

std::optional<BuiltInVectorIcon> GetBuiltInVectorIconType(
    const base::Value::Dict* vector_icon_dict) {
  auto built_in_vector_icon_value =
      vector_icon_dict->FindInt(kBuiltInVectorIcon);
  if (!built_in_vector_icon_value) {
    return std::nullopt;
  }

  auto icon = built_in_vector_icon_value.value();
  if (icon < 0 || icon > static_cast<int>(BuiltInVectorIcon::kMaxValue)) {
    return std::nullopt;
  }
  return static_cast<BuiltInVectorIcon>(icon);
}

std::optional<base::Version> StringToVersion(const std::string* version_value) {
  if (!version_value) {
    return std::nullopt;
  }

  const auto version = base::Version(*version_value);
  if (!version.IsValid()) {
    return std::nullopt;
  }
  return std::move(version);
}

const base::Feature* SelectFeatureByIndex(const base::Feature* features[],
                                          int size,
                                          int index) {
  if (index < 0 || index >= size) {
    // TODO: b/344673533 - Record error metrics.
    return nullptr;
  }

  return features[index];
}

}  // namespace

Trigger::Trigger(TriggerType type) : type(type) {}
Trigger::~Trigger() = default;

Campaigns* GetMutableCampaignsBySlot(CampaignsPerSlot* campaigns_per_slot,
                                     Slot slot) {
  if (!campaigns_per_slot) {
    return nullptr;
  }

  return campaigns_per_slot->FindList(
      base::NumberToString(static_cast<int>(slot)));
}

const Campaigns* GetCampaignsBySlot(const CampaignsPerSlot* campaigns_per_slot,
                                    Slot slot) {
  if (!campaigns_per_slot) {
    return nullptr;
  }
  return campaigns_per_slot->FindList(
      base::NumberToString(static_cast<int>(slot)));
}

const Targetings* GetTargetings(const Campaign* campaign) {
  return campaign->FindList(kTargetings);
}

// Return the payload for the given `slot`. Payload could be nullptr for running
// A/A testing. When payload is nullptr, fallback to the default behavior.
const Payload* GetPayloadBySlot(const Campaign* campaign, Slot slot) {
  switch (slot) {
    case Slot::kDemoModeApp:
      return campaign->FindDictByDottedPath(
          base::StringPrintf(kPayloadPathTemplate, kDemoModePayloadPath));
    case Slot::kNudge:
      return campaign->FindDictByDottedPath(
          base::StringPrintf(kPayloadPathTemplate, kNudgePayloadPath));
    case Slot::kNotification:
      return campaign->FindDictByDottedPath(
          base::StringPrintf(kPayloadPathTemplate, kNotificationPayloadPath));
    case Slot::kOobePerkDiscovery:
      return campaign->FindDictByDottedPath(base::StringPrintf(
          kPayloadPathTemplate, kOobePerkDiscoveryPayloadPath));
    case Slot::kDemoModeFreePlayApps:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return nullptr;
}

std::optional<int> GetCampaignId(const Campaign* campaign) {
  return campaign->FindInt(kId);
}

std::optional<int> GetCampaignGroupId(const Campaign* campaign) {
  return campaign->FindInt(kGroupId);
}

std::optional<int> GetStudyId(const Campaign* campaign) {
  return campaign->FindInt(kStudyId);
}

std::optional<bool> ShouldRegisterTrialWithTriggerEventName(
    const Campaign* campaign) {
  return campaign->FindBool(kShouldRegisterTrialWithTriggerEventName);
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

const base::Value::Dict* TargetingBase::GetDictCriteria(
    const char* path_suffix) const {
  return targeting_->FindDictByDottedPath(GetCriteriaPath(path_suffix));
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

const std::optional<base::Version> DemoModeTargeting::GetAppMinVersion() const {
  return StringToVersion(GetStringCriteria(kMinDemoModeAppVersion));
}

const std::optional<base::Version> DemoModeTargeting::GetAppMaxVersion() const {
  return StringToVersion(GetStringCriteria(kMaxDemoModeAppVersion));
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
  return GetListCriteria(kApplicationLocales);
}

const base::Value::List* DeviceTargeting::GetUserLocales() const {
  return GetListCriteria(kUserLocales);
}

const base::Value::List* DeviceTargeting::GetIncludedCountries() const {
  return GetListCriteria(kIncludedCountries);
}

const base::Value::List* DeviceTargeting::GetExcludedCountries() const {
  return GetListCriteria(kExcludedCountries);
}

const std::optional<int> DeviceTargeting::GetMinMilestone() const {
  return GetIntCriteria(kMinMilestone);
}

const std::optional<int> DeviceTargeting::GetMaxMilestone() const {
  return GetIntCriteria(kMaxMilestone);
}

const std::optional<base::Version> DeviceTargeting::GetMinVersion() const {
  return StringToVersion(GetStringCriteria(kMinVersion));
}

const std::optional<base::Version> DeviceTargeting::GetMaxVersion() const {
  return StringToVersion(GetStringCriteria(kMaxVersion));
}

const std::optional<bool> DeviceTargeting::GetFeatureAwareDevice() const {
  return GetBoolCriteria(kFeatureAware);
}

std::unique_ptr<TimeWindowTargeting> DeviceTargeting::GetRegisteredTime()
    const {
  auto* registered_time_dict = GetDictCriteria(kRegisteredTime);
  if (!registered_time_dict) {
    return nullptr;
  }

  return std::make_unique<TimeWindowTargeting>(registered_time_dict);
}

const std::unique_ptr<NumberRangeTargeting> DeviceTargeting::GetDeviceAge()
    const {
  auto* number_rage_dict = GetDictCriteria(kDeviceAgeInHours);
  if (!number_rage_dict) {
    return nullptr;
  }

  return std::make_unique<NumberRangeTargeting>(number_rage_dict);
}

// Apps Targeting.
AppTargeting::AppTargeting(const base::Value::Dict* app_dict)
    : app_dict_(app_dict) {}

AppTargeting::~AppTargeting() = default;

const std::string* AppTargeting::GetAppId() const {
  return app_dict_->FindString(kAppId);
}

// Events Targeting.
EventsTargeting::EventsTargeting(const base::Value::Dict* config_dict)
    : config_dict_(config_dict) {}

EventsTargeting::~EventsTargeting() = default;

int EventsTargeting::GetImpressionCap() const {
  auto cap = config_dict_->FindInt(kImpressionCap);
  return cap.value_or(kImpressionCapDefaultValue);
}

int EventsTargeting::GetDismissalCap() const {
  auto cap = config_dict_->FindInt(kDismissalCap);
  return cap.value_or(kDismissalCapDefaultValue);
}

std::optional<int> EventsTargeting::GetGroupImpressionCap() const {
  return config_dict_->FindInt(kGroupImpressionCap);
}

std::optional<int> EventsTargeting::GetGroupDismissalCap() const {
  return config_dict_->FindInt(kGroupDismissalCap);
}

const base::Value::List* EventsTargeting::GetEventsConditions() const {
  return config_dict_->FindList(kEventsConditions);
}

// Trigger Targeting.
TriggerTargeting::TriggerTargeting(const base::Value::Dict* trigger_dict)
    : trigger_dict_(trigger_dict) {}

TriggerTargeting::~TriggerTargeting() = default;

std::optional<int> TriggerTargeting::GetTriggerType() const {
  return trigger_dict_->FindInt(kTriggerType);
}

const base::Value::List* TriggerTargeting::GetTriggerEvents() const {
  return trigger_dict_->FindList(kTriggerEvents);
}

// Time window Targeting.
TimeWindowTargeting::TimeWindowTargeting(
    const base::Value::Dict* time_window_dict)
    : time_window_dict_(time_window_dict) {}

TimeWindowTargeting::~TimeWindowTargeting() = default;

const base::Time TimeWindowTargeting::GetStartTime() const {
  auto start = time_window_dict_->FindDouble(kTimeWindowStart);
  if (start.has_value()) {
    return base::Time::FromSecondsSinceUnixEpoch(start.value());
  }

  return base::Time::Min();
}

const base::Time TimeWindowTargeting::GetEndTime() const {
  auto end = time_window_dict_->FindDouble(kTimeWindowEnd);
  if (end.has_value()) {
    return base::Time::FromSecondsSinceUnixEpoch(end.value());
  }

  return base::Time::Max();
}

// Number Range Targeting.
NumberRangeTargeting::NumberRangeTargeting(
    const base::Value::Dict* number_range_dict)
    : number_range_dict_(number_range_dict) {}

NumberRangeTargeting::~NumberRangeTargeting() = default;

const std::optional<int> NumberRangeTargeting::GetStart() const {
  return number_range_dict_->FindInt(kNumberRangeStart);
}

const std::optional<int> NumberRangeTargeting::GetEnd() const {
  return number_range_dict_->FindInt(kNumberRangeEnd);
}

// Session Targeting.
SessionTargeting::SessionTargeting(const Targeting* targeting_dict)
    : TargetingBase(targeting_dict, kSessionTargeting) {}

SessionTargeting::~SessionTargeting() = default;

std::optional<const base::Feature*> SessionTargeting::GetFeature() const {
  const auto one_off_feature_index = GetIntCriteria(kOneOffExpFeatureIndex);
  if (one_off_feature_index) {
    return SelectFeatureByIndex(
        kOneOffFeaturesForExperimentTagTargeting,
        static_cast<int>(std::size(kOneOffFeaturesForExperimentTagTargeting)),
        one_off_feature_index.value());
  }

  const auto predefined_feature_index = GetIntCriteria(kPredefinedFeatureIndex);
  if (predefined_feature_index) {
    return SelectFeatureByIndex(
        kPredefinedFeaturesForExperimentTagTargeting,
        static_cast<int>(
            std::size(kPredefinedFeaturesForExperimentTagTargeting)),
        predefined_feature_index.value());
  }

  return std::nullopt;
}

const base::Value::List* SessionTargeting::GetExperimentTags() const {
  return GetListCriteria(kExperimentTargetings);
}

std::optional<bool> SessionTargeting::GetMinorUser() const {
  return GetBoolCriteria(kMinorUser);
}

std::optional<bool> SessionTargeting::GetIsOwner() const {
  return GetBoolCriteria(kOwner);
}

// Runtime Targeting.
RuntimeTargeting::RuntimeTargeting(const Targeting* targeting_dict)
    : TargetingBase(targeting_dict, kRuntimeTargeting) {}

RuntimeTargeting::~RuntimeTargeting() = default;

const std::vector<std::unique_ptr<TimeWindowTargeting>>
RuntimeTargeting::GetSchedulings() const {
  std::vector<std::unique_ptr<TimeWindowTargeting>> schedulings;
  auto* scheduling_list = GetListCriteria(kSchedulingTargetings);
  if (!scheduling_list) {
    return schedulings;
  }

  for (auto& scheduling_dict : *scheduling_list) {
    if (!scheduling_dict.is_dict()) {
      // Ignore invalid scheduling.
      RecordCampaignsManagerError(CampaignsManagerError::kInvalidScheduling);
      continue;
    }
    schedulings.push_back(
        std::make_unique<TimeWindowTargeting>(&scheduling_dict.GetDict()));
  }
  return schedulings;
}

const std::vector<std::unique_ptr<AppTargeting>>
RuntimeTargeting::GetAppsOpened() const {
  std::vector<std::unique_ptr<AppTargeting>> app_targetings;

  auto* app_targeting_dicts = GetListCriteria(kAppsOpenedTargetings);
  if (!app_targeting_dicts) {
    return app_targetings;
  }

  for (auto& app_targeting_dict : *app_targeting_dicts) {
    if (!app_targeting_dict.is_dict()) {
      RecordCampaignsManagerError(CampaignsManagerError::kInvalidAppTargeting);
      continue;
    }
    app_targetings.push_back(
        std::make_unique<AppTargeting>(&app_targeting_dict.GetDict()));
  }

  return app_targetings;
}

const std::vector<std::string> RuntimeTargeting::GetActiveUrlRegexes() const {
  std::vector<std::string> active_urls_regexs;
  const auto* active_url_regexes_value = GetListCriteria(kActiveUrlRegexes);
  if (!active_url_regexes_value) {
    return active_urls_regexs;
  }
  for (const auto& active_url_regex_value : *active_url_regexes_value) {
    if (!active_url_regex_value.is_string()) {
      // TODO(b/329124927): Record error.
      CAMPAIGNS_LOG(ERROR) << "Invalid active url regex: "
                           << active_url_regex_value.DebugString();
      continue;
    }

    active_urls_regexs.push_back(active_url_regex_value.GetString());
  }

  return active_urls_regexs;
}

std::unique_ptr<EventsTargeting> RuntimeTargeting::GetEventsConfig() const {
  auto* config = GetDictCriteria(kEventsTargetings);
  if (!config) {
    return nullptr;
  }

  return std::make_unique<EventsTargeting>(config);
}

const std::vector<std::unique_ptr<TriggerTargeting>>
RuntimeTargeting::GetTriggers() const {
  std::vector<std::unique_ptr<TriggerTargeting>> triggers;
  auto* triggers_list = GetListCriteria(kTriggerTargetings);
  if (!triggers_list) {
    return triggers;
  }

  for (const auto& trigger : *triggers_list) {
    if (!trigger.is_dict()) {
      // Ignore invalid trigger.
      RecordCampaignsManagerError(CampaignsManagerError::kInvalidTrigger);
      continue;
    }

    triggers.push_back(std::make_unique<TriggerTargeting>(&trigger.GetDict()));
  }
  return triggers;
}

const base::Value::List* RuntimeTargeting::GetUserPrefTargetings() const {
  return GetListCriteria(kUserPrefTargetings);
}

std::unique_ptr<AppTargeting> RuntimeTargeting::GetHotseatAppIcon() const {
  auto* app = GetDictCriteria(kHotseatAppIcon);
  if (!app) {
    return nullptr;
  }
  return std::make_unique<AppTargeting>(app);
}

// Action.
Action::Action(const base::Value::Dict* action_dict)
    : action_dict_(action_dict) {}

Action::~Action() = default;

std::optional<growth::ActionType> Action::GetActionType() const {
  auto action_type_value = action_dict_->FindInt(kActionTypePath);
  if (!action_type_value) {
    CAMPAIGNS_LOG(ERROR) << "Missing action type.";
    RecordCampaignsManagerError(CampaignsManagerError::kMissingActionType);
    return std::nullopt;
  }

  auto action_type = action_type_value.value();
  if (action_type < 0 ||
      action_type > static_cast<int>(growth::ActionType::kMaxValue)) {
    CAMPAIGNS_LOG(ERROR) << "Unrecognized action type.";
    // TODO: b/330931877 - Record an error.
    return std::nullopt;
  }

  return static_cast<growth::ActionType>(action_type);
}

const base::Value::Dict* Action::GetParams() const {
  return action_dict_->FindDict(kActionParamsPath);
}

// Anchor.
Anchor::Anchor(const Targeting* anchor_dict) : anchor_dict_(anchor_dict) {}
Anchor::~Anchor() = default;

const std::optional<WindowAnchorType> Anchor::GetActiveAppWindowAnchorType()
    const {
  if (!anchor_dict_) {
    // No valid anchor dict.
    return std::nullopt;
  }

  const auto anchor_type_value =
      anchor_dict_->FindInt(kActiveAppWindowAnchorType);
  if (!anchor_type_value) {
    // Invalid anchor type.
    CAMPAIGNS_LOG(ERROR) << "Invalid anchor type";
    RecordCampaignsManagerError(CampaignsManagerError::kInvalidAnchorType);
    return std::nullopt;
  }

  auto anchor_type = anchor_type_value.value();
  if (anchor_type < 0 ||
      anchor_type > static_cast<int>(WindowAnchorType::kMaxValue)) {
    return std::nullopt;
  }

  return static_cast<WindowAnchorType>(anchor_type);
}

const std::string* Anchor::GetShelfAppButtonId() const {
  if (!anchor_dict_) {
    // No valid anchor dict.
    return nullptr;
  }

  return anchor_dict_->FindString(kShelfAppButtonId);
}

// Image.
Image::Image(const base::Value::Dict* image_dict) : image_dict_(image_dict) {}
Image::~Image() = default;

const gfx::Image* Image::GetImage() const {
  if (!image_dict_) {
    return nullptr;
  }

  // TODO: b/329113710- Handle other image sources.
  return GetBuiltInImage();
}

const gfx::Image* Image::GetBuiltInImage() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const auto image_id =
      GetBuiltInImageResourceId(GetBuiltInImageType(image_dict_));
  if (image_id) {
    return &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        image_id.value());
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // TODO: b/340895798 - record error metric.
  CAMPAIGNS_LOG(ERROR) << "Unrecognized built in image.";

  return nullptr;
}

// Vector Icon.
VectorIcon::VectorIcon(const base::Value::Dict* vector_icon_dict)
    : vector_icon_dict_(vector_icon_dict) {}
VectorIcon::~VectorIcon() = default;

const gfx::VectorIcon* VectorIcon::GetVectorIcon() const {
  if (!vector_icon_dict_) {
    return nullptr;
  }

  // TODO:b/329113710 - Handle other vector icon sources.
  return GetBuiltInVectorIcon();
}

const gfx::VectorIcon* VectorIcon::GetBuiltInVectorIcon() const {
  const auto icon = GetBuiltInVectorIconType(vector_icon_dict_);
  if (!icon || icon.value() != BuiltInVectorIcon::kRedeem) {
    // TODO: b/340895798 - record error metric.
    CAMPAIGNS_LOG(ERROR) << "Unrecognized built in vector icon.";

    return nullptr;
  }

  return &chromeos::kRedeemIcon;
}

// Image Model.
ImageModel::ImageModel(const base::Value::Dict* image_model_dict)
    : image_model_dict_(image_model_dict) {}
ImageModel::~ImageModel() = default;

const std::optional<ui::ImageModel> ImageModel::GetImageModel() const {
  if (!image_model_dict_) {
    return std::nullopt;
  }

  // TODO(b/329113710): Handle other image sources.
  return GetBuiltInImageModel();
}

const std::optional<ui::ImageModel> ImageModel::GetBuiltInImageModel() const {
  const auto* vector_icon =
      VectorIcon(image_model_dict_->FindDict(kVectorIcon)).GetVectorIcon();
  if (vector_icon) {
    // Returns vector icon.
    return ui::ImageModel::FromVectorIcon(
        *vector_icon, cros_tokens::kCrosSysOnSurface, kIconSize);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const auto* image = Image(image_model_dict_->FindDict(kImage)).GetImage();
  if (image) {
    const gfx::ImageSkia* imageSkia = image->ToImageSkia();
    gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
        *imageSkia, skia::ImageOperations::RESIZE_BEST, kBubbleIconSizeDip);
    resized_image.EnsureRepsForSupportedScales();
    return ui::ImageModel::FromImageSkia(resized_image);
  }
  // TODO: b/340895798 - update the error type naming and description.
  RecordCampaignsManagerError(CampaignsManagerError::kUnrecognizedBuiltInIcon);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  CAMPAIGNS_LOG(ERROR) << "Unrecognized built in image model.";
  return std::nullopt;
}

}  // namespace growth
