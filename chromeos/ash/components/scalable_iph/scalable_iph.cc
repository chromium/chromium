// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace scalable_iph {

namespace {

using NotificationParams =
    ::scalable_iph::ScalableIphDelegate::NotificationParams;
using BubbleParams = ::scalable_iph::ScalableIphDelegate::BubbleParams;
using BubbleIcon = ::scalable_iph::ScalableIphDelegate::BubbleIcon;

constexpr char kFunctionCallAfterKeyedServiceShutdown[] =
    "Function call after keyed service shutdown.";

const base::flat_map<ScalableIph::Event, std::string>& GetEventNamesMap() {
  // IPH events are put in a global namespace. Prefix with ScalableIph for all
  // events.
  static const base::NoDestructor<
      base::flat_map<ScalableIph::Event, std::string>>
      event_names_map({
          {ScalableIph::Event::kFiveMinTick, kEventNameFiveMinTick},
          {ScalableIph::Event::kUnlocked, kEventNameUnlocked},
          {ScalableIph::Event::kAppListShown, kEventNameAppListShown},
      });
  return *event_names_map;
}

std::string GetHelpAppIphEventName(ActionType action_type) {
  switch (action_type) {
    case ActionType::kOpenChrome:
      return kEventNameHelpAppActionTypeOpenChrome;
    case ActionType::kOpenLauncher:
      return kEventNameHelpAppActionTypeOpenLauncher;
    case ActionType::kOpenPersonalizationApp:
      return kEventNameHelpAppActionTypeOpenPersonalizationApp;
    case ActionType::kOpenPlayStore:
      return kEventNameHelpAppActionTypeOpenPlayStore;
    case ActionType::kOpenGoogleDocs:
      return kEventNameHelpAppActionTypeOpenGoogleDocs;
    case ActionType::kOpenGooglePhotos:
      return kEventNameHelpAppActionTypeOpenGooglePhotos;
    case ActionType::kOpenSettingsPrinter:
      return kEventNameHelpAppActionTypeOpenSettingsPrinter;
    case ActionType::kOpenPhoneHub:
      return kEventNameHelpAppActionTypeOpenPhoneHub;
    case ActionType::kOpenYouTube:
      return kEventNameHelpAppActionTypeOpenYouTube;
    case ActionType::kOpenFileManager:
      return kEventNameHelpAppActionTypeOpenFileManager;
    case ActionType::kInvalid:
    default:
      return "";
  }
}

// The list of IPH features `SclableIph` supports. `ScalableIph` checks trigger
// conditions of all events listed in this list when it receives an `Event`.
const std::vector<const base::Feature*>& GetFeatureListConstant() {
  static const base::NoDestructor<std::vector<const base::Feature*>>
      feature_list({
          // This must be sorted from One to Ten. A config expects that IPHs are
          // evaluated in this priority.
          // Timer based.
          &feature_engagement::kIPHScalableIphTimerBasedOneFeature,
          &feature_engagement::kIPHScalableIphTimerBasedTwoFeature,
          &feature_engagement::kIPHScalableIphTimerBasedThreeFeature,
          &feature_engagement::kIPHScalableIphTimerBasedFourFeature,
          &feature_engagement::kIPHScalableIphTimerBasedFiveFeature,
          &feature_engagement::kIPHScalableIphTimerBasedSixFeature,
          &feature_engagement::kIPHScalableIphTimerBasedSevenFeature,
          &feature_engagement::kIPHScalableIphTimerBasedEightFeature,
          &feature_engagement::kIPHScalableIphTimerBasedNineFeature,
          &feature_engagement::kIPHScalableIphTimerBasedTenFeature,
          // Unlocked based.
          &feature_engagement::kIPHScalableIphUnlockedBasedOneFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedTwoFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedThreeFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedFourFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedFiveFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedSixFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedSevenFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedEightFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedNineFeature,
          &feature_engagement::kIPHScalableIphUnlockedBasedTenFeature,
          // Help App based.
          &feature_engagement::kIPHScalableIphHelpAppBasedNudgeFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedOneFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedTwoFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedThreeFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedFourFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedFiveFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedSixFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedSevenFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedEightFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedNineFeature,
          &feature_engagement::kIPHScalableIphHelpAppBasedTenFeature,
      });
  return *feature_list;
}

const base::flat_map<std::string, ActionType>& GetActionTypesMap() {
  // Key will be set in server side config.
  static const base::NoDestructor<base::flat_map<std::string, ActionType>>
      action_types_map({
          {kActionTypeOpenChrome, ActionType::kOpenChrome},
          {kActionTypeOpenLauncher, ActionType::kOpenLauncher},
          {kActionTypeOpenPersonalizationApp,
           ActionType::kOpenPersonalizationApp},
          {kActionTypeOpenPlayStore, ActionType::kOpenPlayStore},
          {kActionTypeOpenGoogleDocs, ActionType::kOpenGoogleDocs},
          {kActionTypeOpenGooglePhotos, ActionType::kOpenGooglePhotos},
          {kActionTypeOpenSettingsPrinter, ActionType::kOpenSettingsPrinter},
          {kActionTypeOpenPhoneHub, ActionType::kOpenPhoneHub},
          {kActionTypeOpenYouTube, ActionType::kOpenYouTube},
          {kActionTypeOpenFileManager, ActionType::kOpenFileManager},
      });
  return *action_types_map;
}

const base::flat_map<std::string, BubbleIcon>& GetBubbleIconsMap() {
  // Key will be set in server side config.
  static const base::NoDestructor<base::flat_map<std::string, BubbleIcon>>
      bubble_icons_map({
          {kBubbleIconChromeIcon, BubbleIcon::kChromeIcon},
          {kBubbleIconPlayStoreIcon, BubbleIcon::kPlayStoreIcon},
          {kBubbleIconGoogleDocsIcon, BubbleIcon::kGoogleDocsIcon},
          {kBubbleIconGooglePhotosIcon, BubbleIcon::kGooglePhotosIcon},
          {kBubbleIconPrintJobsIcon, BubbleIcon::kPrintJobsIcon},
          {kBubbleIconYouTubeIcon, BubbleIcon::kYouTubeIcon},
      });
  return *bubble_icons_map;
}

constexpr base::TimeDelta kTimeTickEventInterval = base::Minutes(5);

std::string GetParamValue(const base::Feature& feature,
                          const std::string& param_name) {
  std::string fully_qualified_param_name =
      base::StrCat({feature.name, "_", param_name});
  std::string value = base::GetFieldTrialParamValueByFeature(
      feature, fully_qualified_param_name);

  // Non-fully-qualified name field must always be empty.
  DCHECK(base::GetFieldTrialParamValueByFeature(feature, param_name).empty())
      << param_name
      << " is specified in a non-fully-qualified way. It should be specified "
         "as "
      << fully_qualified_param_name
      << ". It's often the case in Scalable Iph to enable multiple features at "
         "once. To avoid an unexpected fall-back behavior, non-fully-qualified "
         "name is not accepted. Parameter names of custom fields must be "
         "specified in a fully qualified way: [Feature Name]_[Parameter Name]";

  return value;
}

UiType ParseUiType(const base::Feature& feature) {
  std::string ui_type = GetParamValue(feature, kCustomUiTypeParamName);
  CHECK(ui_type == kCustomUiTypeValueNotification ||
        ui_type == kCustomUiTypeValueBubble ||
        ui_type == kCustomUiTypeValueNone);
  if (ui_type == kCustomUiTypeValueNotification) {
    return UiType::kNotification;
  } else if (ui_type == kCustomUiTypeValueBubble) {
    return UiType::kBubble;
  }
  return UiType::kNone;
}

ActionType ParseActionType(const std::string& action_type_string) {
  auto it = GetActionTypesMap().find(action_type_string);
  if (it == GetActionTypesMap().end()) {
    // If the server side config action type cannot be parsed, will return the
    // kInvalid as the parsed result.
    return ActionType::kInvalid;
  }
  return it->second;
}

std::string ParseActionEventName(const std::string& event_used_param) {
  // The `event_used_param` is in this format:
  // `name:ScalableIphTimerBasedOneEventUsed;comparator:any;window:365;storage:365`.
  auto key_values = base::SplitString(
      event_used_param, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (key_values.size() != 4) {
    return "";
  }
  auto name_value = base::SplitString(key_values[0], ":", base::TRIM_WHITESPACE,
                                      base::SPLIT_WANT_NONEMPTY);
  if (name_value.size() != 2) {
    return "";
  }

  if (name_value[0] != "name") {
    return "";
  }
  return name_value[1];
}

NotificationParams ParseNotificationParams(const base::Feature& feature) {
  // TODO(b/288167957): Implement a fallback for an invalid config, e.g. Do not
  // show an IPH for the case instead of CHECK failure. Config is served from
  // the server. This is not a constraint coming from client side.
  NotificationParams param;
  param.notification_id =
      GetParamValue(feature, kCustomNotificationIdParamName);
  CHECK(!param.notification_id.empty())
      << kCustomNotificationIdParamName << " is a required field";
  param.title = GetParamValue(feature, kCustomNotificationTitleParamName);
  CHECK(!param.title.empty())
      << kCustomNotificationTitleParamName << " is a required field";
  param.text = GetParamValue(feature, kCustomNotificationBodyTextParamName);
  CHECK(!param.text.empty())
      << kCustomNotificationBodyTextParamName << " is a required field";
  param.button.text =
      GetParamValue(feature, kCustomNotificationButtonTextParamName);
  CHECK(!param.button.text.empty())
      << kCustomNotificationButtonTextParamName << " is a required field";
  std::string action_type =
      GetParamValue(feature, kCustomButtonActionTypeParamName);
  CHECK(!action_type.empty()) << kCustomButtonActionTypeParamName
                              << " is a required field for notification";
  param.button.action.action_type = ParseActionType(action_type);
  CHECK(param.button.action.action_type != ActionType::kInvalid)
      << " action type cannot be parsed";
  std::string event_used =
      GetParamValue(feature, kCustomButtonActionEventParamName);
  CHECK(!event_used.empty())
      << kCustomButtonActionEventParamName << " is a required field";
  param.button.action.iph_event_name = ParseActionEventName(event_used);
  CHECK(!event_used.empty()) << " ihp_event_name cannot be parsed";

  std::string image_type =
      GetParamValue(feature, kCustomNotificationImageTypeParamName);
  param.image_type = ScalableIphDelegate::NotificationImageType::kNoImage;
  if (image_type == kCustomNotificationImageTypeValueWallpaper) {
    param.image_type = ScalableIphDelegate::NotificationImageType::kWallpaper;
  }
  return param;
}

BubbleIcon ParseBubbleIcon(const std::string& icon_string) {
  auto it = GetBubbleIconsMap().find(icon_string);
  if (it == GetBubbleIconsMap().end()) {
    // If the server side config bubble icon cannot be parsed, will return the
    // kNoIcon as the parsed result.
    return BubbleIcon::kNoIcon;
  }
  return it->second;
}

BubbleParams ParseBubbleParams(const base::Feature& feature) {
  // TODO(b/288167957): Implement a fallback for an invalid config, e.g. Do not
  // show an IPH for the case instead of CHECK failure. Config is served from
  // the server. This is not a constraint coming from client side.
  BubbleParams param;
  param.bubble_id = GetParamValue(feature, kCustomBubbleIdParamName);
  CHECK(!param.bubble_id.empty())
      << kCustomBubbleIdParamName << " is a required field";
  param.text = GetParamValue(feature, kCustomBubbleTextParamName);
  CHECK(!param.text.empty())
      << kCustomBubbleTextParamName << " is a required field";

  // Button and action:
  // Some nudge may not have a button and action.
  param.button.text = GetParamValue(feature, kCustomBubbleButtonTextParamName);
  if (!param.button.text.empty()) {
    std::string action_type =
        GetParamValue(feature, kCustomButtonActionTypeParamName);
    CHECK(!action_type.empty())
        << kCustomButtonActionTypeParamName << " is a required field";

    param.button.action.action_type = ParseActionType(action_type);
    CHECK(param.button.action.action_type != ActionType::kInvalid)
        << " action type cannot be parsed";

    std::string event_used =
        GetParamValue(feature, kCustomButtonActionEventParamName);
    CHECK(!event_used.empty())
        << kCustomButtonActionEventParamName << " is a required field";
    param.button.action.iph_event_name = ParseActionEventName(event_used);
    CHECK(!event_used.empty()) << " ihp_event_name cannot be parsed";
  }

  auto icon_string = GetParamValue(feature, kCustomBubbleIconParamName);
  param.icon = ParseBubbleIcon(icon_string);

  return param;
}

bool ValidateVersionNumber(const base::Feature& feature) {
  std::string version_number_value =
      GetParamValue(feature, kCustomParamsVersionNumberParamName);
  if (version_number_value.empty()) {
    return false;
  }

  int version_number = 0;
  if (!base::StringToInt(version_number_value, &version_number)) {
    return false;
  }

  return version_number == kCurrentVersionNumber;
}

}  // namespace

ScalableIph::ScalableIph(feature_engagement::Tracker* tracker,
                         std::unique_ptr<ScalableIphDelegate> delegate)
    : tracker_(tracker), delegate_(std::move(delegate)) {
  CHECK(tracker_);
  CHECK(delegate_);

  delegate_observation_.Observe(delegate_.get());

  EnsureTimerStarted();

  online_ = delegate_->IsOnline();

  tracker_->AddOnInitializedCallback(
      base::BindOnce(&ScalableIph::CheckTriggerConditionsOnInitSuccess,
                     weak_ptr_factory_.GetWeakPtr()));
}

ScalableIph::~ScalableIph() = default;

void ScalableIph::Shutdown() {
  timer_.Stop();

  tracker_ = nullptr;

  delegate_observation_.Reset();
  delegate_.reset();
}

void ScalableIph::OnConnectionChanged(bool online) {
  if (online_ == online) {
    return;
  }

  online_ = online;

  tracker_->AddOnInitializedCallback(
      base::BindOnce(&ScalableIph::CheckTriggerConditionsOnInitSuccess,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIph::OnLockStateChanged(bool locked) {
  DCHECK_NE(locked_, locked);
  locked_ = locked;

  if (!locked_) {
    RecordEvent(Event::kUnlocked);
  }
}

void ScalableIph::OnSuspendDoneWithoutLockScreen() {
  DCHECK(!locked_);
  RecordEvent(Event::kUnlocked);
}

void ScalableIph::OnAppListVisibilityChanged(bool shown) {
  if (shown) {
    RecordEvent(Event::kAppListShown);
  }
}

void ScalableIph::PerformActionForIphSession(ActionType action_type) {
  PerformAction(action_type);
}

void ScalableIph::OverrideFeatureListForTesting(
    const std::vector<const base::Feature*> feature_list) {
  CHECK(feature_list_for_testing_.size() == 0)
      << "It's NOT allowed to override feature list twice for testing";
  CHECK(feature_list.size() > 0) << "An empty list is NOT allowed to set.";

  feature_list_for_testing_ = feature_list;
}

void ScalableIph::OverrideTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK(timer_.IsRunning())
      << "Timer is expected to be always running until Shutdown";
  timer_.Stop();
  timer_.SetTaskRunner(task_runner);
  EnsureTimerStarted();
}

void ScalableIph::PerformActionForHelpApp(ActionType action_type) {
  std::string iph_event_name = GetHelpAppIphEventName(action_type);

  // ActionType enum is defined on the client side. We can use CHECK as this is
  // a client side constraint.
  CHECK(!iph_event_name.empty()) << "Unable to resolve the IPH event name to "
                                    "an action type for the help app";

  tracker_->NotifyEvent(iph_event_name);

  PerformAction(action_type);
}

void ScalableIph::PerformAction(ActionType action_type) {
  delegate_->PerformActionForScalableIph(action_type);
}

void ScalableIph::RecordEvent(ScalableIph::Event event) {
  if (!tracker_) {
    DCHECK(false) << kFunctionCallAfterKeyedServiceShutdown;
    return;
  }

  // `AddOnInitializedCallback` immediately calls the callback if it's already
  // initialized.
  tracker_->AddOnInitializedCallback(
      base::BindOnce(&ScalableIph::RecordEventInternal,
                     weak_ptr_factory_.GetWeakPtr(), event));
}

void ScalableIph::EnsureTimerStarted() {
  timer_.Start(FROM_HERE, kTimeTickEventInterval,
               base::BindRepeating(&ScalableIph::RecordTimeTickEvent,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIph::RecordTimeTickEvent() {
  // Do not record timer event when device is locked.
  if (locked_) {
    return;
  }

  RecordEvent(Event::kFiveMinTick);
}

void ScalableIph::RecordEventInternal(ScalableIph::Event event,
                                      bool init_success) {
  if (!tracker_) {
    DCHECK(false) << kFunctionCallAfterKeyedServiceShutdown;
    return;
  }

  if (!init_success) {
    DCHECK(false) << "Failed to initialize feature_engagement::Tracker.";
    return;
  }

  auto it = GetEventNamesMap().find(event);
  if (it == GetEventNamesMap().end()) {
    DCHECK(false) << "Missing ScalableIph::Event to event name string mapping.";
    return;
  }

  tracker_->NotifyEvent(it->second);

  CheckTriggerConditions();
}

void ScalableIph::CheckTriggerConditionsOnInitSuccess(bool init_success) {
  if (!init_success) {
    DCHECK(false) << "Failed to initialize feature_engagement::Tracker.";
    return;
  }

  CheckTriggerConditions();
}

void ScalableIph::CheckTriggerConditions() {
  // Make sure that `tracker_` is initialized. `tracker_` should not cause crash
  // even if we call `ShouldTriggerHelpUI` before initialization. But it returns
  // false. It can become a difficult to notice/debug bug if we accidentally
  // introduce a code path where we call it before initialization.
  DCHECK(tracker_->IsInitialized());

  for (const base::Feature* feature : GetFeatureList()) {
    if (!base::FeatureList::IsEnabled(*feature)) {
      continue;
    }

    if (!ValidateVersionNumber(*feature)) {
      DLOG(WARNING) << "Version number does not match with the current version "
                       "number. Skipping a config: "
                    << feature->name;
      continue;
    }

    if (CheckCustomConditions(*feature) &&
        tracker_->ShouldTriggerHelpUI(*feature)) {
      UiType ui_type = ParseUiType(*feature);
      switch (ui_type) {
        case UiType::kNotification:
          delegate_->ShowNotification(
              ParseNotificationParams(*feature),
              std::make_unique<IphSession>(*feature, tracker_, this));
          break;
        case UiType::kBubble:
          delegate_->ShowBubble(
              ParseBubbleParams(*feature),
              std::make_unique<IphSession>(*feature, tracker_, this));
          break;
        case UiType::kNone:
          break;
      }
    }
  }
}

bool ScalableIph::CheckCustomConditions(const base::Feature& feature) {
  return CheckNetworkConnection(feature) && CheckClientAge(feature);
}

bool ScalableIph::CheckNetworkConnection(const base::Feature& feature) {
  std::string connection_condition =
      GetParamValue(feature, kCustomConditionNetworkConnectionParamName);
  if (connection_condition.empty()) {
    return true;
  }

  // If an invalid value is provided, does not satisfy a condition for a
  // fail-safe behavior.
  if (connection_condition != kCustomConditionNetworkConnectionOnline) {
    DLOG(WARNING) << "Only " << kCustomConditionNetworkConnectionOnline
                  << " is the valid value for network connection condition";
    return false;
  }

  return online_;
}

bool ScalableIph::CheckClientAge(const base::Feature& feature) {
  std::string client_age_condition =
      GetParamValue(feature, kCustomConditionClientAgeInDaysParamName);
  if (client_age_condition.empty()) {
    return true;
  }

  // Use `DLOG`s for logging instead of `DCHECK(false)` as we want to test those
  // fail-safe behaviors in browser_tests.
  int max_client_age = 0;
  if (!base::StringToInt(client_age_condition, &max_client_age)) {
    DLOG(WARNING)
        << "Failed to parse client age condition. It must be an integer.";
    return false;
  }

  if (max_client_age < 0) {
    DLOG(WARNING) << "Client age condition must be a positive integer value.";
    return false;
  }

  int client_age = delegate_->ClientAgeInDays();
  if (client_age < 0) {
    DLOG(WARNING) << "Client age is a negative number. This can happen if a "
                     "user changes time zone, etc. Condition is not satisfied "
                     "for a fail safe behavior.";
    return false;
  }

  return client_age <= max_client_age;
}

const std::vector<const base::Feature*>& ScalableIph::GetFeatureList() const {
  if (!feature_list_for_testing_.empty()) {
    return feature_list_for_testing_;
  }

  return GetFeatureListConstant();
}

}  // namespace scalable_iph
