// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_model.h"

#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/vector_icon_types.h"

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
inline constexpr char kRegisteredTime[] = "registeredTime";
inline constexpr char kDeviceAgeInHours[] = "deviceAgeInHours";

// Session Targeting paths.
inline constexpr char kSessionTargeting[] = "session";

// Experiment Tag Targeting paths.
inline constexpr char kExperimentTargetings[] = "experimentTags";

// User Targeting paths.
inline constexpr char kMinorUser[] = "isMinorUser";
inline constexpr char kOwner[] = "isOwner";

// Events Targeting paths.
inline constexpr char kEventsTargetings[] = "events";
inline constexpr char kImpressionCap[] = "impressionCap";
inline constexpr char kDismissalCap[] = "dismissalCap";
inline constexpr char kEventsConditions[] = "conditions";
inline constexpr int kImpressionCapDefaultValue = 3;
inline constexpr int kDismissalCapDefaultValue = 1;

// Runtime Targeting paths.
inline constexpr char kRuntimeTargeting[] = "runtime";

// Trigger Targeting paths.
inline constexpr char kTriggerTargetings[] = "triggers";

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

// Actions
inline constexpr char kActionTypePath[] = "type";
inline constexpr char kActionParamsPath[] = "params";

// Anchor paths.
inline constexpr char kActiveAppWindowAnchorType[] =
    "activeAppWindowAnchorType";
inline constexpr char kShelfAppButtonId[] = "shelfAppButtonId";

// Image Model.
inline constexpr char kBuiltInIcon[] = "builtInIcon";
inline constexpr int kIconSize = 60;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr gfx::Size kBubbleIconSizeDip = gfx::Size(kIconSize, kIconSize);

std::optional<int> GetBuiltInImageResourceId(
    const std::optional<BuiltInIcon>& icon) {
  if (!icon) {
    return std::nullopt;
  }

  if (icon == BuiltInIcon::kContainerApp) {
    return IDR_GROWTH_FRAMEWORK_CONTAINER_APP_PNG;
  }

  if (icon == BuiltInIcon::kG1) {
    return IDR_GROWTH_FRAMEWORK_G1_PNG;
  }

  return std::nullopt;
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

std::optional<BuiltInIcon> GetBuiltInIconType(
    const base::Value::Dict* image_dict) {
  auto built_in_icon_value = image_dict->FindInt(kBuiltInIcon);
  if (!built_in_icon_value) {
    return std::nullopt;
  }

  return static_cast<BuiltInIcon>(built_in_icon_value.value());
}

}  // namespace

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
    case Slot::kDemoModeFreePlayApps:
      NOTREACHED();
      break;
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

const base::Value::List* EventsTargeting::GetEventsConditions() const {
  return config_dict_->FindList(kEventsConditions);
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
  auto* scheduling_dicts = GetListCriteria(kSchedulingTargetings);
  if (!scheduling_dicts) {
    return schedulings;
  }

  for (auto& scheduling_dict : *scheduling_dicts) {
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

const std::vector<TriggeringType> RuntimeTargeting::GetTriggers() const {
  std::vector<TriggeringType> triggers;
  auto* triggers_list = GetListCriteria(kTriggerTargetings);
  if (!triggers_list) {
    return triggers;
  }

  for (auto& trigger : *triggers_list) {
    if (!trigger.is_int()) {
      // Ignore invalid trigger.
      RecordCampaignsManagerError(CampaignsManagerError::kInvalidTrigger);
      continue;
    }

    // TODO: b/330931877 - Add bounds check for casting to enum from value in
    // campaign payload.
    triggers.push_back(static_cast<TriggeringType>(trigger.GetInt()));
  }
  return triggers;
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
      LOG(ERROR) << "Invalid active url regex: "
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

// Action.
Action::Action(const base::Value::Dict* action_dict)
    : action_dict_(action_dict) {}

Action::~Action() = default;

std::optional<growth::ActionType> Action::GetActionType() const {
  auto action_type_value = action_dict_->FindInt(kActionTypePath);
  if (!action_type_value) {
    LOG(ERROR) << "Missing action type.";
    RecordCampaignsManagerError(CampaignsManagerError::kMissingActionType);
    return std::nullopt;
  }

  return static_cast<growth::ActionType>(action_type_value.value());
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

  const auto anchor_type = anchor_dict_->FindInt(kActiveAppWindowAnchorType);
  if (!anchor_type) {
    // Invalid anchor type.
    LOG(ERROR) << "Invalid anchor type";
    RecordCampaignsManagerError(CampaignsManagerError::kInvalidAnchorType);
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

// Image Model.
Image::Image(const base::Value::Dict* image_dict) : image_dict_(image_dict) {}
Image::~Image() = default;

const gfx::VectorIcon* Image::GetVectorIcon() const {
  const auto icon = GetBuiltInIconType(image_dict_);
  if (!icon || icon.value() != BuiltInIcon::kRedeem) {
    return nullptr;
  }

  return &chromeos::kRedeemIcon;
}

const std::optional<ui::ImageModel> Image::GetImage() const {
  if (!image_dict_) {
    return std::nullopt;
  }

  // TODO(b/329113710): Handle other image sources.
  return GetBuiltInIcon();
}

const std::optional<ui::ImageModel> Image::GetBuiltInIcon() const {
  const auto* vector_icon = GetVectorIcon();
  if (vector_icon) {
    // Returns vector icon.
    return ui::ImageModel::FromVectorIcon(
        *vector_icon, cros_tokens::kCrosSysOnSurface, kIconSize);
  }

  const auto icon = GetBuiltInIconType(image_dict_);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const auto resource_id = GetBuiltInImageResourceId(icon);
  if (resource_id) {
    gfx::ImageSkia* image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            resource_id.value());
    gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
        *image, skia::ImageOperations::RESIZE_BEST, kBubbleIconSizeDip);
    resized_image.EnsureRepsForSupportedScales();
    return ui::ImageModel::FromImageSkia(resized_image);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  RecordCampaignsManagerError(CampaignsManagerError::kUnrecognizedBuiltInIcon);
  LOG(ERROR) << "Unrecognized built in icon: "
             << static_cast<int>(icon.value());
  return std::nullopt;
}

}  // namespace growth
