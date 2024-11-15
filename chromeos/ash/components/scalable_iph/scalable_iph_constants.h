// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_

#include <ostream>

namespace scalable_iph {

inline constexpr char16_t kNotificationSummaryText[] = u"Welcome Tips";

inline constexpr char kScalableIphDebugHost[] = "scalable-iph-debug";
inline constexpr char kScalableIphDebugURL[] =
    "chrome-untrusted://scalable-iph-debug/";

// Those ids are from //chrome/browser/web_applications/web_app_id_constants.h.
// We cannot include the file from this component as //chromeos should not
// depend on //chrome/browser. Those values are tested against values in
// web_app_id_constants.h in `AppListItemActivationWebApp` test. Remember to add
// one if you add new one.
inline constexpr char kWebAppYouTubeAppId[] =
    "agimnkijcaahngcdmfeangaknmldooml";
inline constexpr char kWebAppGoogleDocsAppId[] =
    "mpnpojknpmmopombnjdcgaaiekajbnjb";

// `kWebAppGooglePhotosAppId` is not coming from web_app_id_constants.h.
inline constexpr char kWebAppGooglePhotosAppId[] =
    "ncmjhecbjeaamljdfahankockkkdmedg";

// Android app ids can be found in
// //chrome/browser/ash/app_list/arc/arc_app_utils.cc. We cannot include the
// file from this directory same with the above web_app_id_constants.h.
inline constexpr char kAndroidGooglePhotosAppId[] =
    "fdbkkojdbojonckghlanfaopfakedeca";

// Android app ids can be found in
// //chrome/browser/ash/app_list/arc/arc_app_utils.cc. We cannot include the
// file from this directory same with the above web_app_id_constants.h.
constexpr char kAndroidAppGooglePhotosAppId[] =
    "fdbkkojdbojonckghlanfaopfakedeca";
constexpr char kAndroidAppGooglePlayStoreAppId[] =
    "cnbgggchhmkkdmeppjobngjoejnihlei";

enum class ActionType {
  // `kInvalid` is reserved to be used as an initial value or when the server
  // side config cannot be parsed.
  kInvalid = 0,
  kOpenChrome = 1,
  kOpenLauncher = 2,  // Not implemented for V1 of Scalable IPH
  kOpenPersonalizationApp = 3,
  kOpenPlayStore = 4,
  kOpenGoogleDocs = 5,
  kOpenGooglePhotos = 6,
  kOpenSettingsPrinter = 7,
  kOpenPhoneHub = 8,
  kOpenYouTube = 9,
  kOpenFileManager = 10,
  kOpenHelpAppPerks = 11,
  kOpenChromebookPerksWeb = 12,
  kOpenChromebookPerksGfnPriority2022 = 13,
  kOpenChromebookPerksMinecraft2023 = 14,
  kOpenChromebookPerksMinecraftRealms2023 = 15,
  kLastAction = kOpenChromebookPerksMinecraftRealms2023,
};

std::ostream& operator<<(std::ostream& out, ActionType action_type);

// Constants for action types, has 1 to 1 mapping with the ActionType.
// Used in server side config.
inline constexpr char kActionTypeOpenChrome[] = "OpenChrome";
inline constexpr char kActionTypeOpenLauncher[] = "OpenLauncher";
inline constexpr char kActionTypeOpenPersonalizationApp[] =
    "OpenPersonalizationApp";
inline constexpr char kActionTypeOpenPlayStore[] = "OpenPlayStore";
inline constexpr char kActionTypeOpenGoogleDocs[] = "OpenGoogleDocs";
inline constexpr char kActionTypeOpenGooglePhotos[] = "OpenGooglePhotos";
inline constexpr char kActionTypeOpenSettingsPrinter[] = "OpenSettingsPrinter";
inline constexpr char kActionTypeOpenPhoneHub[] = "OpenPhoneHub";
inline constexpr char kActionTypeOpenYouTube[] = "OpenYouTube";
inline constexpr char kActionTypeOpenFileManager[] = "OpenFileManager";
inline constexpr char kActionTypeOpenHelpAppPerks[] = "OpenHelpAppPerks";
inline constexpr char kActionTypeOpenChromebookPerksWeb[] =
    "OpenChromebookPerksWeb";
inline constexpr char kActionTypeOpenChromebookPerksGfnPriority2022[] =
    "OpenChromebookPerksGfnPriority2022";
inline constexpr char kActionTypeOpenChromebookPerksMinecraft2023[] =
    "OpenChromebookPerksMinecraft2023";
// Use shorter string to keep Finch config payload size smaller.
inline constexpr char kActionTypeOpenChromebookPerksMinecraftRealms2023[] =
    "PerksMinecraftRealms2023";

// Constants for events.
// Naming convention: Camel case starting with a capital letter. Note that
// Scalable Iph event names must start with `ScalableIph` as Iph event names
// live in a global namespace.

// Constants for help app events, has 1 to 1 mapping with the ActionType.
inline constexpr char kEventNameHelpAppActionTypeOpenChrome[] =
    "ScalableIphHelpAppActionOpenChrome";
inline constexpr char kEventNameHelpAppActionTypeOpenLauncher[] =
    "ScalableIphHelpAppActionOpenLauncher";
inline constexpr char kEventNameHelpAppActionTypeOpenPersonalizationApp[] =
    "ScalableIphHelpAppActionOpenPersonalizationApp";
inline constexpr char kEventNameHelpAppActionTypeOpenPlayStore[] =
    "ScalableIphHelpAppActionOpenPlayStore";
inline constexpr char kEventNameHelpAppActionTypeOpenGoogleDocs[] =
    "ScalableIphHelpAppActionOpenGoogleDocs";
inline constexpr char kEventNameHelpAppActionTypeOpenGooglePhotos[] =
    "ScalableIphHelpAppActionOpenGooglePhotos";
inline constexpr char kEventNameHelpAppActionTypeOpenSettingsPrinter[] =
    "ScalableIphHelpAppActionOpenSettingsPrinter";
inline constexpr char kEventNameHelpAppActionTypeOpenPhoneHub[] =
    "ScalableIphHelpAppActionOpenPhoneHub";
inline constexpr char kEventNameHelpAppActionTypeOpenYouTube[] =
    "ScalableIphHelpAppActionOpenYouTube";
inline constexpr char kEventNameHelpAppActionTypeOpenFileManager[] =
    "ScalableIphHelpAppActionOpenFileManager";

// Constants for app list / shelf item activation.
inline constexpr char kEventNameAppListItemActivationYouTube[] =
    "ScalableIphAppListItemActivationYouTube";
inline constexpr char kEventNameAppListItemActivationGoogleDocs[] =
    "ScalableIphAppListItemActivationGoogleDocs";
inline constexpr char kEventNameAppListItemActivationGooglePhotosWeb[] =
    "ScalableIphAppListItemActivationGooglePhotosWeb";
constexpr char kEventNameAppListItemActivationGooglePlayStore[] =
    "ScalableIphAppListItemActivationOpenGooglePlayStore";
constexpr char kEventNameAppListItemActivationGooglePhotosAndroid[] =
    "ScalableIphAppListItemActivationOpenGooglePhotosAndroid";

// Constants for shelf item activation.
inline constexpr char kEventNameShelfItemActivationYouTube[] =
    "ScalableIphShelfItemActivationYouTube";
inline constexpr char kEventNameShelfItemActivationGoogleDocs[] =
    "ScalableIphShelfItemActivationGoogleDocs";
inline constexpr char kEventNameShelfItemActivationGooglePhotosWeb[] =
    "ScalableIphShelfItemActivationGooglePhotosWeb";
inline constexpr char kEventNameShelfItemActivationGooglePhotosAndroid[] =
    "ScalableIphShelfItemActivationGooglePhotosAndroid";
inline constexpr char kEventNameShelfItemActivationGooglePlay[] =
    "ScalableIphShelfItemActivationGooglePlay";

// Recorded when the personalization hub app is opened.
inline constexpr char kEventNameOpenPersonalizationApp[] =
    "ScalableIphOpenPersonalizationApp";
// Recorded when a print job is created.
constexpr char kEventNamePrintJobCreated[] = "ScalableIphPrintJobCreated";

// Recorded when a game window is opened.
inline constexpr char kEventNameGameWindowOpened[] =
    "ScalableIphGameWindowOpened";

// `FiveMinTick` event is recorded every five minutes after OOBE completion.
inline constexpr char kEventNameFiveMinTick[] = "ScalableIphFiveMinTick";

// `Unlocked` event is recorded every unlock of the lock screen or
// `SuspendDone` if the lock screen is not enabled.
inline constexpr char kEventNameUnlocked[] = "ScalableIphUnlocked";

// `AppListShown` event is recorded every time an app list (launcher) becomes
// visible. An expected usage of this event is for `event_used` of an app list
// IPH.
inline constexpr char kEventNameAppListShown[] = "ScalableIphAppListShown";

// All Scalable Iph configs must have version number fields. Scalable Iph
// ignores a config if it does not have a field with a supported version number.
// For now, we guarantee nothing about forward or backward compatibility.
inline constexpr char kCustomParamsVersionNumberParamName[] =
    "x_CustomVersionNumber";
inline constexpr int kCurrentVersionNumber = 1;

// Constants for custom conditions.
// Naming convention:
// Camel case starting with a capital letter. Note that param names must start
// with `x_CustomCondition` prefix:
// - `x_` is from the feature engagement framework. The framework ignores any
//   params start with it.
// - `CustomCondition` indicates this param is for custom condition. We use
//   params for other things as well, e.g. UIs.
//
// Usage:
// Custom conditions is an extension implemented in `ScalableIph` framework.
// Those conditions are checked in addition to other event conditions of the
// feature engagement framework.
//
// Example:
// "x_CustomConditionsNetworkConnection": "Online"
//
// `NetworkConnection` condition is satisfied if a device is online. For now, we
// only support `Online` as the expected condition.
inline constexpr char kCustomConditionNetworkConnectionParamName[] =
    "x_CustomConditionNetworkConnection";
inline constexpr char kCustomConditionNetworkConnectionOnline[] = "Online";

// `ClientAgeInDays` condition is satisfied if a device's client age is on or
// below the specified number of days. The number must be a positive integer
// including 0.
// - The day count starts from 0. For example, if you specify 0 as a value, it
//   means that a profile is created in the last 24 hours.
// - The day in this condition does not match with the calendar day. If a
//   profile is created at 3 pm on May 1st, the day 0 ends at 3 pm on May 2nd.
inline constexpr char kCustomConditionClientAgeInDaysParamName[] =
    "x_CustomConditionClientAgeInDays";

// `HasSavedPrinters` condition is true if there is at least a saved printer.
// Valid values are either `True` or `False`.
inline constexpr char kCustomConditionHasSavedPrintersParamName[] =
    "x_CustomConditionHasSavedPrinter";
inline constexpr char kCustomConditionHasSavedPrintersValueTrue[] = "True";
inline constexpr char kCustomConditionHasSavedPrintersValueFalse[] = "False";

// `PhoneHubOnboardingEligible` condition is true if feature status of phone hub
// is either `kEligiblePhoneButNotSetUp` or `kDisabled`. Note that `kDisabled`
// is a state where a user can enable it from settings. It means that the user
// can set up phone hub. Only `True` is the supported value for now.
inline constexpr char kCustomConditionPhoneHubOnboardingEligibleParamName[] =
    "x_CustomConditionPhoneHubOnboardingEligible";
inline constexpr char kCustomConditionPhoneHubOnboardingEligibleValueTrue[] =
    "True";

// `TriggerEvent` condition is true if an IPH conditions check is triggered by a
// record of an event specified in this condition. Note that only sub-set of
// events can trigger an IPH condition check as specified in
// `kIphTriggeringEvents` in `scalable_iph.cc`.
inline constexpr char kCustomConditionTriggerEventParamName[] =
    "x_CustomConditionTriggerEvent";

// `UiType` param indicates which IPH UI is used for an event config.
inline constexpr char kCustomUiTypeParamName[] = "x_CustomUiType";
inline constexpr char kCustomUiTypeValueNotification[] = "Notification";
inline constexpr char kCustomUiTypeValueBubble[] = "Bubble";
inline constexpr char kCustomUiTypeValueNone[] = "None";

enum class UiType {
  kNotification,
  kBubble,
  kNone,
};

// Parameters for a notification UI. All fields are required field.
// - Notification ID: the id used to add and remove a notification.
// - Title: a title text of a notification.
// - Body text: a body text of a notification.
// - Button text: a text of a button in a notification.
// - Image type: a type of preview image(s) in a notification.
// - Icon: an icon of a notification. Default is Chrome icon.
// - Source text: a source text of a notification. Default is ChromeOS.
// - Summary text: a summary text of a notification. Default is Welcome Tips.
//
// Default value of summary text is set to Welcome Tips as ScalableIph is/was
// primarily implemented/used for Welcome Tips. We can update this behavior
// later with a new version number.
inline constexpr char kCustomNotificationIdParamName[] =
    "x_CustomNotificationId";
inline constexpr char kCustomNotificationTitleParamName[] =
    "x_CustomNotificationTitle";
inline constexpr char kCustomNotificationBodyTextParamName[] =
    "x_CustomNotificationBodyText";
inline constexpr char kCustomNotificationButtonTextParamName[] =
    "x_CustomNotificationButtonText";
inline constexpr char kCustomNotificationImageTypeParamName[] =
    "x_CustomNotificationImageType";
inline constexpr char kCustomNotificationImageTypeValueWallpaper[] =
    "Wallpaper";
inline constexpr char kCustomNotificationImageTypeValueMinecraft[] =
    "Minecraft";
inline constexpr char kCustomNotificationIconParamName[] =
    "x_CustomNotificationIcon";
inline constexpr char kCustomNotificationIconValueDefault[] = "Default";
inline constexpr char kCustomNotificationIconValueRedeem[] = "Redeem";
inline constexpr char kCustomNotificationSourceTextParamName[] =
    "x_CustomNotificationSourceText";
inline constexpr char kCustomNotificationSourceTextValueDefault[] = "ChromeOS";
inline constexpr char kCustomNotificationSummaryTextParamName[] =
    "x_CustomNotificationSummaryText";
inline constexpr char kCustomNotificationSummaryTextValueWelcomeTips[] =
    "WelcomeTips";
inline constexpr char kCustomNotificationSummaryTextValueNone[] = "None";

// Parameters for a bubble UI. All fields are required field.
// - Bubble ID: the id used to add and remove a bubble.
// - Title: the title of a bubble.
// - Text: the text of a bubble.
// - Button text: a text of a button in a bubble.
// Currently only used for the help app nudge:
// - Anchor view app ID: app id of the view to which a bubble is anchored.
inline constexpr char kCustomBubbleIdParamName[] = "x_CustomBubbleId";
inline constexpr char kCustomBubbleTitleParamName[] = "x_CustomBubbleTitle";
inline constexpr char kCustomBubbleTextParamName[] = "x_CustomBubbleText";
inline constexpr char kCustomBubbleButtonTextParamName[] =
    "x_CustomBubbleButtonText";
inline constexpr char kCustomBubbleIconParamName[] = "x_CustomBubbleIcon";
inline constexpr char kCustomBubbleAnchorViewAppIdParamName[] =
    "x_CustomBubbleAnchorViewAppId";

// Constants for bubble icons, has 1 to 1 mapping with the BubbleIcon.
// Used in server side config.
inline constexpr char kBubbleIconChromeIcon[] = "ChromeIcon";
inline constexpr char kBubbleIconPlayStoreIcon[] = "PlayStoreIcon";
inline constexpr char kBubbleIconGoogleDocsIcon[] = "GoogleDocsIcon";
inline constexpr char kBubbleIconGooglePhotosIcon[] = "GooglePhotosIcon";
inline constexpr char kBubbleIconPrintJobsIcon[] = "PrintJobsIcon";
inline constexpr char kBubbleIconYouTubeIcon[] = "YouTubeIcon";

// Parameters for action.
inline constexpr char kCustomButtonActionTypeParamName[] =
    "x_CustomButtonActionType";
inline constexpr char kCustomButtonActionEventParamName[] = "event_used";

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_
