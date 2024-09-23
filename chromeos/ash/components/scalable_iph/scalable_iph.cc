// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph.h"

#include <memory>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/scalable_iph/config.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/logger.h"
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

// A set of ScalableIph events which can trigger an IPH.
constexpr auto kIphTriggeringEvents =
    base::MakeFixedFlatSet<ScalableIph::Event>(
        {ScalableIph::Event::kFiveMinTick, ScalableIph::Event::kUnlocked});

bool force_enable_iph_feature_for_testing = false;

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
const std::vector<raw_ptr<const base::Feature, VectorExperimental>>&
GetFeatureListConstant() {
  static const base::NoDestructor<
      std::vector<raw_ptr<const base::Feature, VectorExperimental>>>
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
          // Gaming.
          &feature_engagement::kIPHScalableIphGamingFeature,
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
          {kActionTypeOpenHelpAppPerks, ActionType::kOpenHelpAppPerks},
          {kActionTypeOpenChromebookPerksWeb,
           ActionType::kOpenChromebookPerksWeb},
          {kActionTypeOpenChromebookPerksGfnPriority2022,
           ActionType::kOpenChromebookPerksGfnPriority2022},
          {kActionTypeOpenChromebookPerksMinecraft2023,
           ActionType::kOpenChromebookPerksMinecraft2023},
          {kActionTypeOpenChromebookPerksMinecraftRealms2023,
           ActionType::kOpenChromebookPerksMinecraftRealms2023},
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

constexpr auto kAppListItemActivationEventsMap =
    base::MakeFixedFlatMap<std::string_view, ScalableIph::Event>({
        {kWebAppGoogleDocsAppId,
         ScalableIph::Event::kAppListItemActivationGoogleDocs},
        {kWebAppYouTubeAppId,
         ScalableIph::Event::kAppListItemActivationYouTube},
        {kWebAppGooglePhotosAppId,
         ScalableIph::Event::kAppListItemActivationGooglePhotosWeb},
        {kAndroidAppGooglePlayStoreAppId,
         ScalableIph::Event::kAppListItemActivationGooglePlayStore},
        {kAndroidAppGooglePhotosAppId,
         ScalableIph::Event::kAppListItemActivationGooglePhotosAndroid},
    });

constexpr auto kShelfItemActivationEventsMap =
    base::MakeFixedFlatMap<std::string_view, ScalableIph::Event>({
        {kWebAppGoogleDocsAppId,
         ScalableIph::Event::kShelfItemActivationGoogleDocs},
        {kWebAppYouTubeAppId, ScalableIph::Event::kShelfItemActivationYouTube},
        {kWebAppGooglePhotosAppId,
         ScalableIph::Event::kShelfItemActivationGooglePhotosWeb},
        {kAndroidGooglePhotosAppId,
         ScalableIph::Event::kShelfItemActivationGooglePhotosAndroid},
    });

constexpr base::TimeDelta kTimeTickEventInterval = base::Minutes(5);

std::string GetEventName(ScalableIph::Event event) {
  // Use switch statement as you can get a compiler error if you forget to add a
  // conversion.
  switch (event) {
    case ScalableIph::Event::kFiveMinTick:
      return kEventNameFiveMinTick;
    case ScalableIph::Event::kUnlocked:
      return kEventNameUnlocked;
    case ScalableIph::Event::kAppListShown:
      return kEventNameAppListShown;
    case ScalableIph::Event::kAppListItemActivationYouTube:
      return kEventNameAppListItemActivationYouTube;
    case ScalableIph::Event::kAppListItemActivationGoogleDocs:
      return kEventNameAppListItemActivationGoogleDocs;
    case ScalableIph::Event::kAppListItemActivationGooglePhotosWeb:
      return kEventNameAppListItemActivationGooglePhotosWeb;
    case ScalableIph::Event::kOpenPersonalizationApp:
      return kEventNameOpenPersonalizationApp;
    case ScalableIph::Event::kShelfItemActivationYouTube:
      return kEventNameShelfItemActivationYouTube;
    case ScalableIph::Event::kShelfItemActivationGoogleDocs:
      return kEventNameShelfItemActivationGoogleDocs;
    case ScalableIph::Event::kShelfItemActivationGooglePhotosWeb:
      return kEventNameShelfItemActivationGooglePhotosWeb;
    case ScalableIph::Event::kShelfItemActivationGooglePhotosAndroid:
      return kEventNameShelfItemActivationGooglePhotosAndroid;
    case ScalableIph::Event::kShelfItemActivationGooglePlay:
      return kEventNameShelfItemActivationGooglePlay;
    case ScalableIph::Event::kAppListItemActivationGooglePlayStore:
      return kEventNameAppListItemActivationGooglePlayStore;
    case ScalableIph::Event::kAppListItemActivationGooglePhotosAndroid:
      return kEventNameAppListItemActivationGooglePhotosAndroid;
    case ScalableIph::Event::kPrintJobCreated:
      return kEventNamePrintJobCreated;
    case ScalableIph::Event::kGameWindowOpened:
      return kEventNameGameWindowOpened;
  }
}

std::string GetParamValue(const base::Feature& feature,
                          const std::string& param_name) {
  std::unique_ptr<Config> config = GetConfig(feature);
  if (config && config->params.contains(param_name)) {
    return config->params.at(param_name);
  }

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

void LogParamValueParseError(Logger* logger,
                             const base::Location& location,
                             const std::string& feature_name,
                             const std::string& param_name) {
  logger->Log(
      location,
      base::StringPrintf(
          "%s does not have a valid %s param value. Stop parsing the config.",
          feature_name.c_str(), param_name.c_str()));
}

UiType ParseUiType(Logger* logger, const base::Feature& feature) {
  std::string ui_type = GetParamValue(feature, kCustomUiTypeParamName);
  if (ui_type != kCustomUiTypeValueNotification &&
      ui_type != kCustomUiTypeValueBubble &&
      ui_type != kCustomUiTypeValueNone) {
    SCALABLE_IPH_LOG(logger) << ui_type << " is not a valid UI type.";
  }

  if (ui_type == kCustomUiTypeValueNotification) {
    return UiType::kNotification;
  }

  if (ui_type == kCustomUiTypeValueBubble) {
    return UiType::kBubble;
  }

  return UiType::kNone;
}

UiType GetUiType(Logger* logger, const base::Feature& feature) {
  std::unique_ptr<Config> config = GetConfig(feature);
  if (config) {
    return config->ui_type;
  }

  return ParseUiType(logger, feature);
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

ScalableIphDelegate::NotificationIcon GetNotificationIcon(
    const std::string& icon) {
  if (icon == kCustomNotificationIconValueRedeem) {
    return ScalableIphDelegate::NotificationIcon::kRedeem;
  }

  return ScalableIphDelegate::NotificationIcon::kDefault;
}

ScalableIphDelegate::NotificationSummaryText GetNotificationSummaryText(
    const std::string& summary_text) {
  if (summary_text == kCustomNotificationSummaryTextValueNone) {
    return ScalableIphDelegate::NotificationSummaryText::kNone;
  }

  return ScalableIphDelegate::NotificationSummaryText::kWelcomeTips;
}

std::unique_ptr<NotificationParams> ParseNotificationParams(
    Logger* logger,
    const base::Feature& feature) {
  std::unique_ptr<NotificationParams> param =
      std::make_unique<NotificationParams>();
  param->notification_id =
      GetParamValue(feature, kCustomNotificationIdParamName);
  if (param->notification_id.empty()) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomNotificationIdParamName);
    return nullptr;
  }
  param->title = GetParamValue(feature, kCustomNotificationTitleParamName);
  if (param->title.empty()) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomNotificationTitleParamName);
    return nullptr;
  }

  // Notification body text is an optional field. This can take an empty string.
  param->text = GetParamValue(feature, kCustomNotificationBodyTextParamName);

  param->button.text =
      GetParamValue(feature, kCustomNotificationButtonTextParamName);
  if (param->button.text.empty()) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomNotificationButtonTextParamName);
    return nullptr;
  }
  std::string action_type =
      GetParamValue(feature, kCustomButtonActionTypeParamName);
  if (action_type.empty()) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomButtonActionTypeParamName);
    return nullptr;
  }
  param->button.action.action_type = ParseActionType(action_type);
  if (param->button.action.action_type == ActionType::kInvalid) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomButtonActionTypeParamName);
    return nullptr;
  }
  std::string event_used =
      GetParamValue(feature, kCustomButtonActionEventParamName);
  if (event_used.empty()) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomButtonActionEventParamName);
    return nullptr;
  }
  param->button.action.iph_event_name = ParseActionEventName(event_used);
  if (param->button.action.iph_event_name.empty()) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomButtonActionEventParamName);
    return nullptr;
  }

  std::string image_type =
      GetParamValue(feature, kCustomNotificationImageTypeParamName);
  param->image_type = ScalableIphDelegate::NotificationImageType::kNoImage;
  if (image_type == kCustomNotificationImageTypeValueWallpaper) {
    param->image_type = ScalableIphDelegate::NotificationImageType::kWallpaper;
  } else if (image_type == kCustomNotificationImageTypeValueMinecraft) {
    param->image_type = ScalableIphDelegate::NotificationImageType::kMinecraft;
  }

  std::string icon = GetParamValue(feature, kCustomNotificationIconParamName);
  if (!icon.empty()) {
    param->icon = GetNotificationIcon(icon);
  }
  SCALABLE_IPH_LOG(logger) << kCustomNotificationIconParamName
                           << " is specified as " << icon << ". " << param->icon
                           << " is set.";

  std::string summary_text =
      GetParamValue(feature, kCustomNotificationSummaryTextParamName);
  if (!summary_text.empty()) {
    param->summary_text = GetNotificationSummaryText(summary_text);
  }
  SCALABLE_IPH_LOG(logger) << kCustomNotificationSummaryTextParamName
                           << " is specified as " << summary_text << ". "
                           << param->summary_text << " is set.";

  std::string source =
      GetParamValue(feature, kCustomNotificationSourceTextParamName);
  if (!source.empty()) {
    param->source = source;
  } else {
    param->source = kCustomNotificationSourceTextValueDefault;
  }
  SCALABLE_IPH_LOG(logger) << kCustomNotificationSourceTextParamName
                           << " is specified as " << source << ". "
                           << param->source << " is set.";

  return param;
}

std::unique_ptr<NotificationParams> GetNotificationParams(
    Logger* logger,
    const base::Feature& feature) {
  std::unique_ptr<Config> config = GetConfig(feature);
  if (config) {
    return std::move(config->notification_params);
  }

  return ParseNotificationParams(logger, feature);
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

std::unique_ptr<BubbleParams> ParseBubbleParams(Logger* logger,
                                                const base::Feature& feature) {
  std::unique_ptr<BubbleParams> param = std::make_unique<BubbleParams>();
  param->bubble_id = GetParamValue(feature, kCustomBubbleIdParamName);
  if (param->bubble_id.empty()) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomBubbleIdParamName);
    return nullptr;
  }
  // Title of bubble could be empty.
  param->title = GetParamValue(feature, kCustomBubbleTitleParamName);
  param->text = GetParamValue(feature, kCustomBubbleTextParamName);
  if (param->text.empty()) {
    LogParamValueParseError(logger, FROM_HERE, feature.name,
                            kCustomBubbleTextParamName);
    return nullptr;
  }

  // Button and action:
  // Some nudge may not have a button and action.
  param->button.text = GetParamValue(feature, kCustomBubbleButtonTextParamName);
  if (!param->button.text.empty()) {
    std::string action_type =
        GetParamValue(feature, kCustomButtonActionTypeParamName);
    if (action_type.empty()) {
      LogParamValueParseError(logger, FROM_HERE, feature.name,
                              kCustomButtonActionTypeParamName);
      return nullptr;
    }

    param->button.action.action_type = ParseActionType(action_type);
    if (param->button.action.action_type == ActionType::kInvalid) {
      LogParamValueParseError(logger, FROM_HERE, feature.name,
                              kCustomButtonActionTypeParamName);
      return nullptr;
    }

    std::string event_used =
        GetParamValue(feature, kCustomButtonActionEventParamName);
    if (event_used.empty()) {
      LogParamValueParseError(logger, FROM_HERE, feature.name,
                              kCustomButtonActionEventParamName);
      return nullptr;
    }
    param->button.action.iph_event_name = ParseActionEventName(event_used);
    if (param->button.action.iph_event_name.empty()) {
      LogParamValueParseError(logger, FROM_HERE, feature.name,
                              kCustomButtonActionEventParamName);
      return nullptr;
    }
  }

  auto icon_string = GetParamValue(feature, kCustomBubbleIconParamName);
  param->icon = ParseBubbleIcon(icon_string);
  param->anchor_view_app_id =
      GetParamValue(feature, kCustomBubbleAnchorViewAppIdParamName);

  return param;
}

std::unique_ptr<BubbleParams> GetBubbleParams(Logger* logger,
                                              const base::Feature& feature) {
  std::unique_ptr<Config> config = GetConfig(feature);
  if (config) {
    return std::move(config->bubble_params);
  }

  return ParseBubbleParams(logger, feature);
}

bool ValidateVersionNumber(const base::Feature& feature) {
  std::unique_ptr<Config> config = GetConfig(feature);
  if (config) {
    return config->version_number == kCurrentVersionNumber;
  }

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

// static
bool ScalableIph::IsAnyIphFeatureEnabled() {
  if (force_enable_iph_feature_for_testing) {
    return true;
  }

  const std::vector<raw_ptr<const base::Feature, VectorExperimental>>&
      feature_list = GetFeatureListConstant();
  for (auto feature : feature_list) {
    if (base::FeatureList::IsEnabled(*feature)) {
      return true;
    }
  }
  return false;
}

// static
void ScalableIph::ForceEnableIphFeatureForTesting() {
  CHECK_IS_TEST();
  CHECK(!force_enable_iph_feature_for_testing)
      << "Iph feature is already force enabled";

  force_enable_iph_feature_for_testing = true;
}

ScalableIph::ScalableIph(feature_engagement::Tracker* tracker,
                         std::unique_ptr<ScalableIphDelegate> delegate,
                         std::unique_ptr<Logger> logger)
    : tracker_(tracker),
      delegate_(std::move(delegate)),
      logger_(std::move(logger)) {
  CHECK(tracker_);
  CHECK(delegate_);
  CHECK(logger_);

  delegate_observation_.Observe(delegate_.get());

  EnsureTimerStarted();

  online_ = delegate_->IsOnline();

  SCALABLE_IPH_LOG(GetLogger()) << "Initialize: Online: " << online_;

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

  SCALABLE_IPH_LOG(GetLogger())
      << "Connection status changed. Online: " << online;

  tracker_->AddOnInitializedCallback(
      base::BindOnce(&ScalableIph::CheckTriggerConditionsOnInitSuccess,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIph::OnSessionStateChanged(
    ScalableIphDelegate::SessionState session_state) {
  if (session_state_ == session_state) {
    // Note that `OnSessionStateChanged` can be called more than once with the
    // same `session_state` as `session_manager::SessionState` does not map to
    // `ScalableIphDelegate::SessionState` with a 1:1 mapping, e.g.
    // `ScalableIphDelegate::SessionState::kOther` is mapped to several states
    // of `session_manager::SessionState`.
    return;
  }

  const bool unlocked =
      session_state_ == ScalableIphDelegate::SessionState::kLocked &&
      session_state != ScalableIphDelegate::SessionState::kLocked;

  session_state_ = session_state;

  SCALABLE_IPH_LOG(GetLogger())
      << "Session state changed to " << session_state
      << ". Whether this is considered to be an unlocked event or not: "
      << unlocked;

  if (unlocked) {
    RecordEvent(Event::kUnlocked);
  }

  if (session_state_ == ScalableIphDelegate::SessionState::kActive) {
    // Run conditions check as an IPH might be shown after a login.
    tracker_->AddOnInitializedCallback(
        base::BindOnce(&ScalableIph::CheckTriggerConditionsOnInitSuccess,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ScalableIph::OnSuspendDoneWithoutLockScreen() {
  if (session_state_ == ScalableIphDelegate::SessionState::kLocked) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Unexpected ScalableIph::OnSuspendDoneWithoutLockScreen call";
    DCHECK(false) << "OnSuspendDoneWithoutLockScreen should never be called "
                     "with a lock screen";
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Recording kUnlocked because of OnSuspendDoneWithoutLockScreen";
  RecordEvent(Event::kUnlocked);
}

void ScalableIph::OnAppListVisibilityChanged(bool shown) {
  SCALABLE_IPH_LOG(GetLogger())
      << "App list visibility changed. Shown: " << shown;

  if (shown) {
    RecordEvent(Event::kAppListShown);
  }
}

void ScalableIph::OnHasSavedPrintersChanged(bool has_saved_printers) {
  DCHECK_NE(has_saved_printers_, has_saved_printers);

  has_saved_printers_ = has_saved_printers;

  SCALABLE_IPH_LOG(GetLogger())
      << "Has saved printers status changed. Has saved printers: "
      << has_saved_printers;

  if (!has_saved_printers_closure_for_testing_.is_null()) {
    has_saved_printers_closure_for_testing_.Run();
    has_saved_printers_closure_for_testing_.Reset();
  }
}

void ScalableIph::OnPhoneHubOnboardingEligibleChanged(
    bool phonehub_onboarding_eligible) {
  DCHECK_NE(phonehub_onboarding_eligible_, phonehub_onboarding_eligible);

  SCALABLE_IPH_LOG(GetLogger())
      << "Phonehub onboarding eligible state has "
         "changed: Phone hub onboarding eligible: from: "
      << phonehub_onboarding_eligible_
      << " to: " << phonehub_onboarding_eligible;

  phonehub_onboarding_eligible_ = phonehub_onboarding_eligible;
}

void ScalableIph::PerformActionForIphSession(ActionType action_type) {
  SCALABLE_IPH_LOG(GetLogger())
      << "Performing an action for an iph session. Action type:" << action_type;
  PerformAction(action_type);
}

void ScalableIph::MaybeRecordAppListItemActivation(const std::string& id) {
  auto it = kAppListItemActivationEventsMap.find(id);
  if (it == kAppListItemActivationEventsMap.end()) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Observed an app list item activation. But not recording an app "
           "list item activation as it's not listed in the map.";
    return;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Recording an app list item activation as event: " << it->second;
  // Record an event via `RecordEvent` instead of directly notifying an event to
  // `tracker_` as `RecordEvent` can do common tasks, e.g. Making sure that a
  // `tracker_` is initialized, etc.
  RecordEvent(it->second);
}

void ScalableIph::MaybeRecordShelfItemActivationById(const std::string& id) {
  auto it = kShelfItemActivationEventsMap.find(id);
  if (it == kShelfItemActivationEventsMap.end()) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Observed a shelf item activation. But not recording a shelf item "
           "activation as it's not listed in the map.";
    return;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Recording a shelf item activation as event: " << it->second;
  RecordEvent(it->second);
}

void ScalableIph::OverrideFeatureListForTesting(
    const std::vector<raw_ptr<const base::Feature, VectorExperimental>>
        feature_list) {
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

const std::vector<raw_ptr<const base::Feature, VectorExperimental>>&
ScalableIph::GetFeatureListConstantForTesting() {
  CHECK_IS_TEST();
  return GetFeatureListConstant();
}

bool ScalableIph::ShouldPinHelpAppToShelf() {
  return ash::features::AreHelpAppWelcomeTipsEnabled();
}

void ScalableIph::PerformActionForHelpApp(ActionType action_type) {
  SCALABLE_IPH_LOG(GetLogger())
      << "Perform action for help app. Action type: " << action_type;

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

void ScalableIph::SetHasSavedPrintersChangedClosureForTesting(
    base::RepeatingClosure has_saved_printers_closure) {
  CHECK(has_saved_printers_closure_for_testing_.is_null());
  has_saved_printers_closure_for_testing_ =
      std::move(has_saved_printers_closure);
}

void ScalableIph::RecordEvent(ScalableIph::Event event) {
  SCALABLE_IPH_LOG(GetLogger()) << "Record event. Event: " << event;

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

Logger* ScalableIph::GetLogger() {
  return logger_.get();
}

void ScalableIph::EnsureTimerStarted() {
  timer_.Start(FROM_HERE, kTimeTickEventInterval,
               base::BindRepeating(&ScalableIph::RecordTimeTickEvent,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIph::RecordTimeTickEvent() {
  // Do not record timer event outside of an active session, e.g. OOBE, lock
  // screen.
  if (session_state_ != ScalableIphDelegate::SessionState::kActive) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Observed time tick event. But not recording it as session state is "
           "not Active. Current session state is: "
        << session_state_;
    return;
  }

  SCALABLE_IPH_LOG(GetLogger()) << "Record time tick event.";
  RecordEvent(Event::kFiveMinTick);
}

void ScalableIph::RecordEventInternal(ScalableIph::Event event,
                                      bool init_success) {
  if (!tracker_) {
    DCHECK(false) << kFunctionCallAfterKeyedServiceShutdown;
    return;
  }

  if (!init_success) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Failed to initialize feature_engagement::Tracker";
    DCHECK(false) << "Failed to initialize feature_engagement::Tracker.";
    return;
  }

  if (session_state_ != ScalableIphDelegate::SessionState::kActive) {
    SCALABLE_IPH_LOG(GetLogger())
        << "No event is expected to be recorded outside of an active session.";
    return;
  }

  const std::string event_name = GetEventName(event);
  SCALABLE_IPH_LOG(GetLogger()) << "Recording event as " << event_name;
  tracker_->NotifyEvent(event_name);

  if (kIphTriggeringEvents.contains(event)) {
    SCALABLE_IPH_LOG(GetLogger()) << event
                                  << " is a condition check triggering event. "
                                     "Running trigger conditions check.";
    CheckTriggerConditions(event);
  }
}

void ScalableIph::CheckTriggerConditionsOnInitSuccess(bool init_success) {
  if (!init_success) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Failed to initialize feature_engagement::Tracker.";
    return;
  }

  CheckTriggerConditions(std::nullopt);
}

void ScalableIph::CheckTriggerConditions(
    const std::optional<ScalableIph::Event>& trigger_event) {
  // Make sure that `tracker_` is initialized. `tracker_` should not cause crash
  // even if we call `ShouldTriggerHelpUI` before initialization. But it returns
  // false. It can become a difficult to notice/debug bug if we accidentally
  // introduce a code path where we call it before initialization.
  DCHECK(tracker_->IsInitialized());

  if (session_state_ != ScalableIphDelegate::SessionState::kActive) {
    SCALABLE_IPH_LOG(GetLogger()) << "Session state is not Active. No trigger "
                                     "condition check. Session state is "
                                  << session_state_;
    return;
  }

  SCALABLE_IPH_LOG(GetLogger()) << "Running trigger conditions check.";
  for (const base::Feature* feature : GetFeatureList()) {
    SCALABLE_IPH_LOG(GetLogger()) << "Checking: " << feature->name;

    if (!base::FeatureList::IsEnabled(*feature)) {
      SCALABLE_IPH_LOG(GetLogger())
          << feature->name << " is not enabled. Skipping condition check.";
      continue;
    }

    if (!ValidateVersionNumber(*feature)) {
      SCALABLE_IPH_LOG(GetLogger())
          << "Version number does not match with the current version "
             "number. Skipping a config: "
          << feature->name;
      continue;
    }

    if (!CheckCustomConditions(*feature, trigger_event)) {
      SCALABLE_IPH_LOG(GetLogger())
          << "Custom conditions are not satisfied for " << feature->name;
      continue;
    }
    SCALABLE_IPH_LOG(GetLogger())
        << "Custom conditions are satisfied for " << feature->name;

    if (!tracker_->ShouldTriggerHelpUI(*feature)) {
      SCALABLE_IPH_LOG(GetLogger())
          << "Trigger conditions in feature_engagement::Tracker are not "
             "satisfied for "
          << feature->name;
      continue;
    }
    SCALABLE_IPH_LOG(GetLogger())
        << "Trigger conditions in feature_engagement::Tracker are satisfied "
           "for "
        << feature->name;

    UiType ui_type = GetUiType(GetLogger(), *feature);
    switch (ui_type) {
      case UiType::kNotification: {
        std::unique_ptr<NotificationParams> notification_params =
            GetNotificationParams(GetLogger(), *feature);
        if (!notification_params) {
          SCALABLE_IPH_LOG(GetLogger())
              << "Failed to parse notification params for " << feature->name
              << ". Skipping the config.";
          continue;
        }
        SCALABLE_IPH_LOG(GetLogger()) << "Triggering a notification.";

        if (delegate_->ShowNotification(
                *notification_params.get(),
                std::make_unique<IphSession>(*feature, tracker_, this))) {
          SCALABLE_IPH_LOG(GetLogger())
              << "Requested the UI framework to show a notification. Request "
                 "status: success. -> Do not check other trigger conditions to "
                 "avoid triggering multiple IPHs at the same time.";
          return;
        }

        SCALABLE_IPH_LOG(GetLogger())
            << "Requested the UI framework to show a notification. Request "
               "status: failure. -> Keep checking other trigger conditions as "
               "this IPH should not be shown.";
        continue;
      }
      case UiType::kBubble: {
        std::unique_ptr<BubbleParams> bubble_params =
            GetBubbleParams(GetLogger(), *feature);
        if (!bubble_params) {
          SCALABLE_IPH_LOG(GetLogger())
              << "Failed to parse bubble params for " << feature->name
              << ". Skipping the config.";
          continue;
        }
        SCALABLE_IPH_LOG(GetLogger()) << "Triggering a bubble.";
        if (delegate_->ShowBubble(
                *bubble_params.get(),
                std::make_unique<IphSession>(*feature, tracker_, this))) {
          SCALABLE_IPH_LOG(GetLogger())
              << "Requested the UI framework to show a bubble. Request status: "
                 "success. -> Do not check other trigger conditions to avoid "
                 "triggering multiple IPHs at the same time.";
          return;
        }

        SCALABLE_IPH_LOG(GetLogger())
            << "Requested the UI framework to show a bubble. Request status: "
               "failure. -> Keep checking other trigger conditions as this IPH "
               "should not be shown.";
        continue;
      }
      case UiType::kNone:
        SCALABLE_IPH_LOG(GetLogger())
            << "Condition gets satisfied. But specified ui type is None.";
        break;
    }
  }
}

bool ScalableIph::CheckCustomConditions(
    const base::Feature& feature,
    const std::optional<ScalableIph::Event>& trigger_event) {
  SCALABLE_IPH_LOG(GetLogger())
      << "Checking custom conditions for " << feature.name;
  return CheckTriggerEvent(feature, trigger_event) &&
         CheckNetworkConnection(feature) && CheckClientAge(feature) &&
         CheckHasSavedPrinters(feature) &&
         CheckPhoneHubOnboardingEligible(feature);
}

bool ScalableIph::CheckTriggerEvent(
    const base::Feature& feature,
    const std::optional<ScalableIph::Event>& trigger_event) {
  if (!trigger_event.has_value()) {
    SCALABLE_IPH_LOG(GetLogger())
        << "This condition check is NOT triggered by an event. Skipping this "
           "trigger event condition check.";
    return true;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Checking trigger event condition for " << feature.name;

  std::string trigger_event_condition =
      GetParamValue(feature, kCustomConditionTriggerEventParamName);
  if (trigger_event_condition.empty()) {
    SCALABLE_IPH_LOG(GetLogger()) << "No trigger event condition specified.";
    return true;
  }

  std::string trigger_event_name = GetEventName(trigger_event.value());

  const bool result = trigger_event_condition == trigger_event_name;
  SCALABLE_IPH_LOG(GetLogger())
      << "Specified trigger event name is " << trigger_event_condition
      << ". This condition check is triggered by " << trigger_event.value()
      << ". Compared trigger event name is " << trigger_event_name
      << ". Result: " << result;
  return result;
}

bool ScalableIph::CheckNetworkConnection(const base::Feature& feature) {
  SCALABLE_IPH_LOG(GetLogger())
      << "Checking network condition for " << feature.name;
  std::string connection_condition =
      GetParamValue(feature, kCustomConditionNetworkConnectionParamName);
  if (connection_condition.empty()) {
    SCALABLE_IPH_LOG(GetLogger()) << "No network condition specified.";
    return true;
  }

  // If an invalid value is provided, does not satisfy a condition for a
  // fail-safe behavior.
  if (connection_condition != kCustomConditionNetworkConnectionOnline) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Only " << kCustomConditionNetworkConnectionOnline
        << " is the valid value for network connection condition";
    return false;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Expecting online. Current status is: Online: " << online_;
  return online_;
}

bool ScalableIph::CheckClientAge(const base::Feature& feature) {
  SCALABLE_IPH_LOG(GetLogger()) << "Checking client age for " << feature.name;
  std::string client_age_condition =
      GetParamValue(feature, kCustomConditionClientAgeInDaysParamName);
  if (client_age_condition.empty()) {
    SCALABLE_IPH_LOG(GetLogger()) << "No client age condition specified.";
    return true;
  }

  // Use `SCALABLE_IPH_LOG`s for logging instead of `DCHECK(false)` as we want
  // to test those fail-safe behaviors in browser_tests.
  int max_client_age = 0;
  if (!base::StringToInt(client_age_condition, &max_client_age)) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Failed to parse client age condition. It must be an integer.";
    return false;
  }

  if (max_client_age < 0) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Client age condition must be a positive integer value.";
    return false;
  }

  int client_age = delegate_->ClientAgeInDays();
  if (client_age < 0) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Client age is a negative number. This can happen if a "
           "user changes time zone, etc. Condition is not satisfied "
           "for a fail safe behavior.";
    return false;
  }

  const bool result = client_age <= max_client_age;
  SCALABLE_IPH_LOG(GetLogger())
      << "Current client age is " << client_age
      << ". Specified max client age is " << max_client_age
      << " (inclusive). Condition satisfied is: " << result;
  return result;
}

bool ScalableIph::CheckHasSavedPrinters(const base::Feature& feature) {
  SCALABLE_IPH_LOG(GetLogger())
      << "Checking has saved printers condition for " << feature.name;
  std::string has_saved_printers_condition =
      GetParamValue(feature, kCustomConditionHasSavedPrintersParamName);
  if (has_saved_printers_condition.empty()) {
    SCALABLE_IPH_LOG(GetLogger())
        << "No has saved printers condition specified.";
    return true;
  }

  if (has_saved_printers_condition !=
          kCustomConditionHasSavedPrintersValueTrue &&
      has_saved_printers_condition !=
          kCustomConditionHasSavedPrintersValueFalse) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Invalid value provided for "
        << kCustomConditionHasSavedPrintersParamName
        << ". This condition is not satisfied for a fail-safe behavior.";
    return false;
  }

  const bool expected_value =
      has_saved_printers_condition == kCustomConditionHasSavedPrintersValueTrue;
  const bool result = has_saved_printers_ == expected_value;
  SCALABLE_IPH_LOG(GetLogger())
      << "Expected value is " << expected_value
      << ". Current has saved printers value is " << has_saved_printers_
      << ". Result is " << result;
  return result;
}

bool ScalableIph::CheckPhoneHubOnboardingEligible(
    const base::Feature& feature) {
  SCALABLE_IPH_LOG(GetLogger())
      << "Checking phone hub onboarding eligible for " << feature.name;

  std::string phonehub_onboarding_eligible_value = GetParamValue(
      feature, kCustomConditionPhoneHubOnboardingEligibleParamName);
  if (phonehub_onboarding_eligible_value.empty()) {
    SCALABLE_IPH_LOG(GetLogger())
        << "No phone hub onboarding eligible condition specified.";
    return true;
  }

  if (phonehub_onboarding_eligible_value !=
      kCustomConditionPhoneHubOnboardingEligibleValueTrue) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Only " << kCustomConditionPhoneHubOnboardingEligibleValueTrue
        << " is a valid value for "
        << kCustomConditionPhoneHubOnboardingEligibleParamName
        << ". Provided value: " << phonehub_onboarding_eligible_value
        << ". Condition not satisfied for a fail-safe behavior.";
    return false;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Expected value is "
      << kCustomConditionPhoneHubOnboardingEligibleValueTrue
      << ". Current phone hub onboarding eligible value is "
      << phonehub_onboarding_eligible_ << ". Result is "
      << phonehub_onboarding_eligible_;
  return phonehub_onboarding_eligible_;
}

const std::vector<raw_ptr<const base::Feature, VectorExperimental>>&
ScalableIph::GetFeatureList() const {
  if (!feature_list_for_testing_.empty()) {
    return feature_list_for_testing_;
  }

  return GetFeatureListConstant();
}

std::ostream& operator<<(std::ostream& out, ScalableIph::Event event) {
  return out << GetEventName(event);
}

}  // namespace scalable_iph
