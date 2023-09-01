// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_

#include <ostream>

namespace scalable_iph {

constexpr char kScalableIphDebugHost[] = "scalable-iph-debug";
constexpr char kScalableIphDebugURL[] =
    "chrome-untrusted://scalable-iph-debug/";

// Those ids are from //chrome/browser/web_applications/web_app_id_constants.h.
// We cannot include the file from this component as //chromeos should not
// depend on //chrome/browser. Those values are tested against values in
// web_app_id_constants.h in `AppListItemActivationWebApp` test. Remember to add
// one if you add new one.
constexpr char kWebAppYouTubeAppId[] = "agimnkijcaahngcdmfeangaknmldooml";
constexpr char kWebAppGoogleDocsAppId[] = "mpnpojknpmmopombnjdcgaaiekajbnjb";

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
  kLastAction = kOpenFileManager,
};

std::ostream& operator<<(std::ostream& out, ActionType action_type);

// Constants for action types, has 1 to 1 mapping with the ActionType.
// Used in server side config.
constexpr char kActionTypeOpenChrome[] = "OpenChrome";
constexpr char kActionTypeOpenLauncher[] = "OpenLauncher";
constexpr char kActionTypeOpenPersonalizationApp[] = "OpenPersonalizationApp";
constexpr char kActionTypeOpenPlayStore[] = "OpenPlayStore";
constexpr char kActionTypeOpenGoogleDocs[] = "OpenGoogleDocs";
constexpr char kActionTypeOpenGooglePhotos[] = "OpenGooglePhotos";
constexpr char kActionTypeOpenSettingsPrinter[] = "OpenSettingsPrinter";
constexpr char kActionTypeOpenPhoneHub[] = "OpenPhoneHub";
constexpr char kActionTypeOpenYouTube[] = "OpenYouTube";
constexpr char kActionTypeOpenFileManager[] = "OpenFileManager";

// Constants for events.
// Naming convention: Camel case starting with a capital letter. Note that
// Scalable Iph event names must start with `ScalableIph` as Iph event names
// live in a global namespace.

// Constants for help app events, has 1 to 1 mapping with the ActionType.
constexpr char kEventNameHelpAppActionTypeOpenChrome[] =
    "ScalableIphHelpAppActionOpenChrome";
constexpr char kEventNameHelpAppActionTypeOpenLauncher[] =
    "ScalableIphHelpAppActionOpenLauncher";
constexpr char kEventNameHelpAppActionTypeOpenPersonalizationApp[] =
    "ScalableIphHelpAppActionOpenPersonalizationApp";
constexpr char kEventNameHelpAppActionTypeOpenPlayStore[] =
    "ScalableIphHelpAppActionOpenPlayStore";
constexpr char kEventNameHelpAppActionTypeOpenGoogleDocs[] =
    "ScalableIphHelpAppActionOpenGoogleDocs";
constexpr char kEventNameHelpAppActionTypeOpenGooglePhotos[] =
    "ScalableIphHelpAppActionOpenGooglePhotos";
constexpr char kEventNameHelpAppActionTypeOpenSettingsPrinter[] =
    "ScalableIphHelpAppActionOpenSettingsPrinter";
constexpr char kEventNameHelpAppActionTypeOpenPhoneHub[] =
    "ScalableIphHelpAppActionOpenPhoneHub";
constexpr char kEventNameHelpAppActionTypeOpenYouTube[] =
    "ScalableIphHelpAppActionOpenYouTube";
constexpr char kEventNameHelpAppActionTypeOpenFileManager[] =
    "ScalableIphHelpAppActionOpenFileManager";

// Constants for app list item activation in the launcher.
constexpr char kEventNameAppListItemActivationYouTube[] =
    "ScalableIphAppListItemActivationYouTube";
constexpr char kEventNameAppListItemActivationGoogleDocs[] =
    "ScalableIphAppListItemActivationGoogleDocs";

// `FiveMinTick` event is recorded every five minutes after OOBE completion.
constexpr char kEventNameFiveMinTick[] = "ScalableIphFiveMinTick";

// `Unlocked` event is recorded every unlock of the lock screen or
// `SuspendDone` if the lock screen is not enabled.
constexpr char kEventNameUnlocked[] = "ScalableIphUnlocked";

// `AppListShown` event is recorded every time an app list (launcher) becomes
// visible. An expected usage of this event is for `event_used` of an app list
// IPH.
constexpr char kEventNameAppListShown[] = "ScalableIphAppListShown";

// All Scalable Iph configs must have version number fields. Scalable Iph
// ignores a config if it does not have a field with a supported version number.
// For now, we guarantee nothing about forward or backward compatibility.
constexpr char kCustomParamsVersionNumberParamName[] = "x_CustomVersionNumber";
constexpr int kCurrentVersionNumber = 1;

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
constexpr char kCustomConditionNetworkConnectionParamName[] =
    "x_CustomConditionNetworkConnection";
constexpr char kCustomConditionNetworkConnectionOnline[] = "Online";

// `ClientAgeInDays` condition is satisfied if a device's client age is on or
// below the specified number of days. The number must be a positive integer
// including 0.
// - The day count starts from 0. For example, if you specify 0 as a value, it
//   means that a profile is created in the last 24 hours.
// - The day in this condition does not match with the calendar day. If a
//   profile is created at 3 pm on May 1st, the day 0 ends at 3 pm on May 2nd.
constexpr char kCustomConditionClientAgeInDaysParamName[] =
    "x_CustomConditionClientAgeInDays";

// `HasSavedPrinters` condition is true if there is at least a saved printer.
// Valid values are either `True` or `False`.
constexpr char kCustomConditionHasSavedPrintersParamName[] =
    "x_CustomConditionHasSavedPrinter";
constexpr char kCustomConditionHasSavedPrintersValueTrue[] = "True";
constexpr char kCustomConditionHasSavedPrintersValueFalse[] = "False";

// `UiType` param indicates which IPH UI is used for an event config.
constexpr char kCustomUiTypeParamName[] = "x_CustomUiType";
constexpr char kCustomUiTypeValueNotification[] = "Notification";
constexpr char kCustomUiTypeValueBubble[] = "Bubble";
constexpr char kCustomUiTypeValueNone[] = "None";

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
constexpr char kCustomNotificationIdParamName[] = "x_CustomNotificationId";
constexpr char kCustomNotificationTitleParamName[] =
    "x_CustomNotificationTitle";
constexpr char kCustomNotificationBodyTextParamName[] =
    "x_CustomNotificationBodyText";
constexpr char kCustomNotificationButtonTextParamName[] =
    "x_CustomNotificationButtonText";
constexpr char kCustomNotificationImageTypeParamName[] =
    "x_CustomNotificationImageType";
constexpr char kCustomNotificationImageTypeValueWallpaper[] = "Wallpaper";

// Parameters for a bubble UI. All fields are required field.
// - Bubble ID: the id used to add and remove a bubble.
// - Title: the title of a bubble.
// - Text: the text of a bubble.
// - Button text: a text of a button in a bubble.
// Currently only used for the help app nudge:
// - Anchor view app ID: app id of the view to which a bubble is anchored.
constexpr char kCustomBubbleIdParamName[] = "x_CustomBubbleId";
constexpr char kCustomBubbleTitleParamName[] = "x_CustomBubbleTitle";
constexpr char kCustomBubbleTextParamName[] = "x_CustomBubbleText";
constexpr char kCustomBubbleButtonTextParamName[] = "x_CustomBubbleButtonText";
constexpr char kCustomBubbleIconParamName[] = "x_CustomBubbleIcon";
constexpr char kCustomBubbleAnchorViewAppIdParamName[] =
    "x_CustomBubbleAnchorViewAppId";

// Constants for bubble icons, has 1 to 1 mapping with the BubbleIcon.
// Used in server side config.
constexpr char kBubbleIconChromeIcon[] = "ChromeIcon";
constexpr char kBubbleIconPlayStoreIcon[] = "PlayStoreIcon";
constexpr char kBubbleIconGoogleDocsIcon[] = "GoogleDocsIcon";
constexpr char kBubbleIconGooglePhotosIcon[] = "GooglePhotosIcon";
constexpr char kBubbleIconPrintJobsIcon[] = "PrintJobsIcon";
constexpr char kBubbleIconYouTubeIcon[] = "YouTubeIcon";

// Parameters for action.
constexpr char kCustomButtonActionTypeParamName[] = "x_CustomButtonActionType";
constexpr char kCustomButtonActionEventParamName[] = "event_used";

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_
