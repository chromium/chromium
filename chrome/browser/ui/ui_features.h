// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// limited to top chrome UI.

#ifndef CHROME_BROWSER_UI_UI_FEATURES_H_
#define CHROME_BROWSER_UI_UI_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

BASE_DECLARE_FEATURE(kAllowEyeDropperWGCScreenCapture);

BASE_DECLARE_FEATURE(kCreateNewTabGroupAppMenuTopLevel);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kDseIntegrity);
BASE_DECLARE_FEATURE(kFewerUpdateConfirmations);
#endif

BASE_DECLARE_FEATURE(kEnableExtensionsMenuTeardownFix);

BASE_DECLARE_FEATURE(kImportExportFlags);

// All feature flags associated with Glow Up
BASE_DECLARE_FEATURE(kTabStripDeclutter);
BASE_DECLARE_FEATURE(kToolbarGlowUp);
BASE_DECLARE_FEATURE(kRoundedIcons);
BASE_DECLARE_FEATURE(kMenuSimplification);
BASE_DECLARE_FEATURE(kTabGroupColorRefresh);
BASE_DECLARE_FEATURE(kWebuiRefresh2026);

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Controls how extensions show up in the main menu. When enabled, if the
// current profile has no extensions, instead of a full extensions submenu, only
// the "Discover Chrome Extensions" item will be present.
BASE_DECLARE_FEATURE(kExtensionsCollapseMainMenu);

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kPdfInfoBar);
BASE_DECLARE_FEATURE(kSeparateDefaultAndPinPrompt);
BASE_DECLARE_FEATURE_PARAM(int, kSeparateDefaultAndPinPromptRandSeed);
BASE_DECLARE_FEATURE_PARAM(int, kSeparateDefaultAndPinPromptPinMaxCount);
BASE_DECLARE_FEATURE_PARAM(int, kSeparateDefaultAndPinPromptPinCooldownDays);
BASE_DECLARE_FEATURE_PARAM(int, kSeparateDefaultAndPinPromptDefaultMaxCount);
BASE_DECLARE_FEATURE_PARAM(int,
                           kSeparateDefaultAndPinPromptDefaultCooldownDays);
BASE_DECLARE_FEATURE_PARAM(int, kSeparateDefaultAndPinPromptMessageVersion);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// When enabled, user may see the session restore UI flow.
BASE_DECLARE_FEATURE(kSessionRestoreInfobar);

// When this param is true, the session restore preference will have
// continue where you left off as default behavior
BASE_DECLARE_FEATURE_PARAM(bool, kSetDefaultToContinueSession);
#endif

BASE_DECLARE_FEATURE(kPreloadTopChromeWebUI);
// This enum entry values must be in sync with
// WebUIContentsPreloadManager::PreloadMode.
enum class PreloadTopChromeWebUIMode {
  kPreloadOnWarmup = 0,
  kPreloadOnMakeContents = 1
};

inline constexpr char kPreloadTopChromeWebUIModeName[] = "preload-mode";

inline constexpr char kPreloadTopChromeWebUIModePreloadOnWarmupName[] =
    "preload-on-warmup";

inline constexpr char kPreloadTopChromeWebUIModePreloadOnMakeContentsName[] =
    "preload-on-make-contents";

inline constexpr base::FeatureParam<PreloadTopChromeWebUIMode>::Option
    kPreloadTopChromeWebUIModeOptions[] = {
        {PreloadTopChromeWebUIMode::kPreloadOnWarmup,
         kPreloadTopChromeWebUIModePreloadOnWarmupName},
        {PreloadTopChromeWebUIMode::kPreloadOnMakeContents,
         kPreloadTopChromeWebUIModePreloadOnMakeContentsName}};

inline constexpr base::FeatureParam<PreloadTopChromeWebUIMode>
    kPreloadTopChromeWebUIMode(&kPreloadTopChromeWebUI,
                               kPreloadTopChromeWebUIModeName,
                               PreloadTopChromeWebUIMode::kPreloadOnWarmup,
                               &kPreloadTopChromeWebUIModeOptions);

// If smart preload is enabled, the preload WebUI is determined by historical
// engagement scores and whether a WebUI is currently being shown.
// If disabled, always preload Tab Search.
inline constexpr char kPreloadTopChromeWebUISmartPreloadName[] =
    "smart-preload";

inline constexpr base::FeatureParam<bool> kPreloadTopChromeWebUISmartPreload(
    &kPreloadTopChromeWebUI,
    kPreloadTopChromeWebUISmartPreloadName,
    true);

// If delay preload is enabled, the preloading is delayed until the first
// non empty paint of an observed web contents.
//
// In case of browser startup, the observed web contents is the active web
// contents of the last created browser.
//
// In case of Request() is called, the requested web contents is observed.
//
// In case of web contents destroy, the preloading simply waits for a fixed
// amount of time.
inline constexpr char kPreloadTopChromeWebUIDelayPreloadName[] =
    "delay-preload";

inline constexpr base::FeatureParam<bool> kPreloadTopChromeWebUIDelayPreload(
    &kPreloadTopChromeWebUI,
    kPreloadTopChromeWebUIDelayPreloadName,
    true);

// An list of exclude origins for WebUIs that don't participate in preloading.
// The list is a string of format "<origin>,<origin2>,...,<origin-n>", where
// each <origin> is a WebUI origin, e.g. "chrome://tab-search.top-chrome". This
// is used for emergency preloading shutoff for problematic WebUIs.
inline constexpr char kPreloadTopChromeWebUIExcludeOriginsName[] =
    "exclude-origins";

inline constexpr base::FeatureParam<std::string>
    kPreloadTopChromeWebUIExcludeOrigins(
        &kPreloadTopChromeWebUI,
        kPreloadTopChromeWebUIExcludeOriginsName,
        "");

BASE_DECLARE_FEATURE(kPreloadTopChromeWebUILessNavigations);

BASE_DECLARE_FEATURE(kPressAndHoldEscToExitBrowserFullscreen);

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kProcessIsolationSettings);
#endif  // BUILDFLAG(IS_WIN)

BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kShowDropTargetForTabDelay);

// Overrides the `kSplitViewTabDraggingUpdates` feature flag if set.
// The drop target is only shown if the mouse hasn't moved a certain distance
// over a period of time. The timer and distance used scales linearly with the
// size of the drop target.
BASE_DECLARE_FEATURE(kSplitViewDragAndDropVelocity);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSplitViewDragAndDropMinDelay);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSplitViewDragAndDropMaxDelay);
BASE_DECLARE_FEATURE_PARAM(int, kSplitViewDragAndDropMinDistanceThreshold);
BASE_DECLARE_FEATURE_PARAM(int, kSplitViewDragAndDropMaxDistanceThreshold);

BASE_DECLARE_FEATURE(kTabDuplicateMetrics);

BASE_DECLARE_FEATURE(kTabGroupsCollapseFreezing);

#if !BUILDFLAG(IS_ANDROID)
// General improvements to tab group menus

BASE_DECLARE_FEATURE(kTabGroupMenuMoreEntryPoints);
bool IsTabGroupMenuMoreEntryPointsEnabled();

BASE_DECLARE_FEATURE(kTabGroupHoverCards);
bool IsTabGroupHoverCardsEnabled();

#endif  // !BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kTabHoverCardImages);

// These parameters control how long the hover card system waits before
// requesting a preview image from a tab where no preview image is available.
// Values are in ms.
inline constexpr char kTabHoverCardImagesNotReadyDelayParameterName[] =
    "page_not_ready_delay";

inline constexpr char kTabHoverCardImagesLoadingDelayParameterName[] =
    "page_loading_delay";

inline constexpr char kTabHoverCardImagesLoadedDelayParameterName[] =
    "page_loaded_delay";

// Determines how long to wait during a hover card slide transition before a
// placeholder image is displayed via crossfade.
// -1: disable crossfade entirely
//  0: show placeholder immediately
//  1: show placeholder when the card lands on the new tab
//  between 0 and 1: show at a percentage of transition
//
// Note: crossfade is automatically disabled if animations are disabled at the
// OS level (e.g. for accessibility).
inline constexpr char kTabHoverCardImagesCrossfadePreviewAtParameterName[] =
    "crossfade_preview_at";

// Adds an amount of time (in ms) to the show delay when tabs are max width -
// typically when there are less than 5 or 6 tabs in a browser window.
inline constexpr char kTabHoverCardAdditionalMaxWidthDelay[] =
    "additional_max_width_delay";

// If enabled, use desktop widget to show tab modal dialogs.
BASE_DECLARE_FEATURE(kTabModalUsesDesktopWidget);

BASE_DECLARE_FEATURE(kTearOffWebAppTabOpensWebAppWindow);

#if !BUILDFLAG(IS_ANDROID)
// Enables a three-button password save dialog variant (essentially adding a
// "not now" button alongside "never").
BASE_DECLARE_FEATURE(kThreeButtonPasswordSaveDialog);

// Enables a split button for the "Cancel" action in the Password Save bubble.
BASE_DECLARE_FEATURE(kPasswordSaveUpdateDropdownMenuExperiment);
#endif

// Feature which uses a flyover animation for animating side panels (and
// expansion/contraction of the Vertical Tab Strip).
//
// Call `UseSidePanelFlyoverAnimation()` instead of checking this feature
// directly.
BASE_DECLARE_FEATURE(kSidePanelFlyoverAnimation);
bool UseSidePanelFlyoverAnimation();

BASE_DECLARE_FEATURE_PARAM(int, kSidePanelFlyoverDurationMs);
BASE_DECLARE_FEATURE_PARAM(bool, kSidePanelFlyoverUseDefaultDeadline);

// TODO(crbug.com/460764864): Cleanup all the enterprise badging feature flags.
BASE_DECLARE_FEATURE(kEnterpriseProfileBadgingForMenu);
BASE_DECLARE_FEATURE(kNTPFooterBadgingPolicies);

BASE_DECLARE_FEATURE(kEnterpriseManagementDisclaimerUsesCustomLabel);
BASE_DECLARE_FEATURE(kManagedProfileRequiredInterstitial);

// Cocoa to views migration.
#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kViewsJSAppModalDialog);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kUsePortalAccentColor);
#endif

// Controls whether the site-specific data dialog shows a related installed
// applications section.
BASE_DECLARE_FEATURE(kPageSpecificDataDialogRelatedInstalledAppsSection);

// Feature for the promotion banner on the top of chrome://management page
BASE_DECLARE_FEATURE(kEnableManagementPromotionBanner);

// Controls whether a performance improvement in browser feature support
// checking is enabled.
BASE_DECLARE_FEATURE(kInlineFullscreenPerfExperiment);

// Controls whether the new page actions framework should be displaying page
// actions.
BASE_DECLARE_FEATURE(kPageActionsMigration);

// For development only, set this to enable all page actions.
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationEnableAll);

// The following feature params indicate whether individual features should
// have their page actions controlled using the new framework.
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationIntentPicker);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationZoom);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationFileSystemAccess);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationCookieControls);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationAutofillMandatoryReauth);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationSharingHub);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationAiMode);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationVirtualCard);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationFilledCardInformation);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationReadingMode);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationSavePayments);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationLensOverlayHomework);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationBookmarkStar);

BASE_DECLARE_FEATURE(kPageActionsPrioritySelector);

// Determines whether the "save password" page action displays different UI if
// the user has said to never save passwords for that site.
BASE_DECLARE_FEATURE(kSavePasswordsContextualUi);

#if BUILDFLAG(IS_MAC)
// Add tab group colours when viewing tab groups using the top mac OS menu bar.
BASE_DECLARE_FEATURE(kShowTabGroupsMacSystemMenu);
#endif  // BUILDFLAG(IS_MAC)

// If enabled, the by date history will show in the side panel.
BASE_DECLARE_FEATURE(kByDateHistoryInSidePanel);

// If enabled, the "Tabs from other devices" side panel will be available.
BASE_DECLARE_FEATURE(kTabsFromOtherDevicesSidePanel);

// If enabled, Stable-channel instances of Chrome will be hidden from the "Tabs
// from other devices" side panel.
BASE_DECLARE_FEATURE(kTabsFromOtherDevicesSidePanelExcludeStableChannel);

// If enabled, the "Tabs from other devices" toolbar button will be pinned by
// default.
BASE_DECLARE_FEATURE(kTabsFromOtherDevicesSidePanelPinnedByDefault);

#if !BUILDFLAG(IS_ANDROID)
// Controls whether to add new tabs to active tab group or to the end of the
// tab strip.
BASE_DECLARE_FEATURE(kNewTabAddsToActiveGroup);

bool IsNewTabAddsToActiveGroupEnabled();

bool IsWebUIReloadButtonEnabled();

bool IsWebUIHomeButtonEnabled();

bool IsWebUIBackForwardButtonEnabled();

bool IsWebUIPinnedToolbarActionsEnabled();

bool IsWebUIExtensionsContainerEnabled();

bool IsWebUISplitTabsButtonEnabled();

// Controls whether the WebUI version of the Avatar Button is used.
BASE_DECLARE_FEATURE(kWebUIAvatarButton);
bool IsWebUIAvatarButtonEnabled();

bool IsWebUIAppMenuButtonEnabled();

bool IsWebUILocationBarEnabled();

bool IsWebUIToolbarEnabled();
#endif  // !BUILDFLAG(IS_ANDROID)

// Controls whether to show a toast for Chrome non milestone update.
BASE_DECLARE_FEATURE(kNonMilestoneUpdateToast);

// Controls whether the updated bookmark and tab group conversion is enabled.
BASE_DECLARE_FEATURE(kBookmarkTabGroupConversion);

bool IsBookmarkTabGroupConversionEnabled();

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kAndroidAnimatedProgressBarInBrowser);

bool IsAndroidAnimatedProgressBarInBrowserEnabled();
#endif  // BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kAiOverlayDialog);
BASE_DECLARE_FEATURE_PARAM(std::string, kAiOverlayDialogApiKey);
BASE_DECLARE_FEATURE_PARAM(std::string, kAiOverlayDialogMockJsonPath);

BASE_DECLARE_FEATURE(kTabGroupsFocusing);
BASE_DECLARE_FEATURE_PARAM(bool, kTabGroupsFocusingPinnedTabs);
BASE_DECLARE_FEATURE_PARAM(bool, kTabGroupsFocusingAutoClose);
BASE_DECLARE_FEATURE_PARAM(bool, kTabGroupsFocusingDefaultToFocused);

BASE_DECLARE_FEATURE(kVerticalTabsGrabHandleRemoval);
BASE_DECLARE_FEATURE_PARAM(bool, kVerticalTabsGrabHandleRemovalAlways);

}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
