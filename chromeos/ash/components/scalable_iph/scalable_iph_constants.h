// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_

namespace scalable_iph {

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
inline constexpr char kEventNameAppListItemActivationGooglePlayStore[] =
    "ScalableIphAppListItemActivationOpenGooglePlayStore";
inline constexpr char kEventNameAppListItemActivationGooglePhotosAndroid[] =
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
inline constexpr char kEventNamePrintJobCreated[] =
    "ScalableIphPrintJobCreated";

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

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_
