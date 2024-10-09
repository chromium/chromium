// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// limited to top chrome UI.

#ifndef CHROME_BROWSER_UI_UI_FEATURES_H_
#define CHROME_BROWSER_UI_UI_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// TODO(crbug.com/40598679): Remove this when the tab dragging
// interactive_ui_tests pass on Wayland.
BASE_DECLARE_FEATURE(kAllowWindowDragUsingSystemDragDrop);

BASE_DECLARE_FEATURE(kAllowEyeDropperWGCScreenCapture);

BASE_DECLARE_FEATURE(kChromeLabs);
extern const char kChromeLabsActivationParameterName[];
extern const base::FeatureParam<int> kChromeLabsActivationPercentage;

BASE_DECLARE_FEATURE(kCloseOmniboxPopupOnInactiveAreaClick);

BASE_DECLARE_FEATURE(kDefaultBrowserPromptRefresh);
BASE_DECLARE_FEATURE(kDefaultBrowserPromptRefreshTrial);

// String representation of the study group for running a synthetic trial.
extern const base::FeatureParam<std::string>
    kDefaultBrowserPromptRefreshStudyGroup;

// Whether to show the default browser info bar prompt.
extern const base::FeatureParam<bool> kShowDefaultBrowserInfoBar;

// Whether to show the default browser app menu chip prompt.
extern const base::FeatureParam<bool> kShowDefaultBrowserAppMenuChip;

// Whether to show the default browser app menu item anytime the browser isn't
// default, even if the app menu chip prompt isn't enabled.
extern const base::FeatureParam<bool> kShowDefaultBrowserAppMenuItem;

// Whether to show the updated info bar strings.
extern const base::FeatureParam<bool> kUpdatedInfoBarCopy;

// Base duration after which the user may be remprompted.
extern const base::FeatureParam<base::TimeDelta> kRepromptDuration;

// Maximum number of times a user will be prompted. When set to a negative
// value, the user will be prompted indefinitely.
extern const base::FeatureParam<int> kMaxPromptCount;

// Exponential backoff multiplier for the reprompt duration.
extern const base::FeatureParam<int> kRepromptDurationMultiplier;

// The duration after which the app menu prompt should not longer be shown.
extern const base::FeatureParam<base::TimeDelta> kDefaultBrowserAppMenuDuration;

// Whether the app menu chip should use more prominent colors.
extern const base::FeatureParam<bool> kAppMenuChipColorPrimary;

BASE_DECLARE_FEATURE(kExtensionsMenuInAppMenu);
bool IsExtensionMenuInRootAppMenu();

#if !defined(ANDROID)
BASE_DECLARE_FEATURE(kAccessCodeCastUI);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kFewerUpdateConfirmations);
#endif

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
BASE_DECLARE_FEATURE(kIOSPromoRefreshedPasswordBubble);

BASE_DECLARE_FEATURE(kIOSPromoAddressBubble);

BASE_DECLARE_FEATURE(kIOSPromoPaymentBubble);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kHaTSWebUI);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
BASE_DECLARE_FEATURE(kLightweightExtensionOverrideConfirmations);
#endif

BASE_DECLARE_FEATURE(kPreloadTopChromeWebUI);
// This enum entry values must be in sync with
// WebUIContentsPreloadManager::PreloadMode.
enum class PreloadTopChromeWebUIMode {
  kPreloadOnWarmup = 0,
  kPreloadOnMakeContents = 1
};
extern const char kPreloadTopChromeWebUIModeName[];
extern const char kPreloadTopChromeWebUIModePreloadOnWarmupName[];
extern const char kPreloadTopChromeWebUIModePreloadOnMakeContentsName[];
extern const base::FeatureParam<PreloadTopChromeWebUIMode>
    kPreloadTopChromeWebUIMode;

// If smart preload is enabled, the preload WebUI is determined by historical
// engagement scores and whether a WebUI is currently being shown.
// If disabled, always preload Tab Search.
extern const char kPreloadTopChromeWebUISmartPreloadName[];
extern const base::FeatureParam<bool> kPreloadTopChromeWebUISmartPreload;

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
extern const char kPreloadTopChromeWebUIDelayPreloadName[];
extern const base::FeatureParam<bool> kPreloadTopChromeWebUIDelayPreload;

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kPressAndHoldEscToExitBrowserFullscreen);
#endif

BASE_DECLARE_FEATURE(kResponsiveToolbar);

BASE_DECLARE_FEATURE(kTabDuplicateMetrics);

BASE_DECLARE_FEATURE(kTabScrollingButtonPosition);
extern const char kTabScrollingButtonPositionParameterName[];

BASE_DECLARE_FEATURE(kSidePanelWebView);

#if !defined(ANDROID)
BASE_DECLARE_FEATURE(kSidePanelCompanionDefaultPinned);
#endif

BASE_DECLARE_FEATURE(kSidePanelJourneysQueryless);
BASE_DECLARE_FEATURE(kSidePanelResizing);
BASE_DECLARE_FEATURE(kSidePanelSearchCompanion);

BASE_DECLARE_FEATURE(kSideSearch);
BASE_DECLARE_FEATURE(kSideSearchFeedback);
BASE_DECLARE_FEATURE(kSearchWebInSidePanel);

BASE_DECLARE_FEATURE(kTabGroupsCollapseFreezing);

BASE_DECLARE_FEATURE(kTabHoverCardImages);

// These parameters control how long the hover card system waits before
// requesting a preview image from a tab where no preview image is available.
// Values are in ms.
extern const char kTabHoverCardImagesNotReadyDelayParameterName[];
extern const char kTabHoverCardImagesLoadingDelayParameterName[];
extern const char kTabHoverCardImagesLoadedDelayParameterName[];

// Determines how long to wait during a hover card slide transition before a
// placeholder image is displayed via crossfade.
// -1: disable crossfade entirely
//  0: show placeholder immediately
//  1: show placeholder when the card lands on the new tab
//  between 0 and 1: show at a percentage of transition
//
// Note: crossfade is automatically disabled if animations are disabled at the
// OS level (e.g. for accessibility).
extern const char kTabHoverCardImagesCrossfadePreviewAtParameterName[];

// Adds an amount of time (in ms) to the show delay when tabs are max width -
// typically when there are less than 5 or 6 tabs in a browser window.
extern const char kTabHoverCardAdditionalMaxWidthDelay[];

BASE_DECLARE_FEATURE(kTabOrganization);
bool IsTabOrganization();

BASE_DECLARE_FEATURE(kTabstripDeclutter);
bool IsTabstripDeclutterEnabled();

BASE_DECLARE_FEATURE(kMultiTabOrganization);

BASE_DECLARE_FEATURE(kTabOrganizationAppMenuItem);

BASE_DECLARE_FEATURE(kTabReorganization);

BASE_DECLARE_FEATURE(kTabReorganizationDivider);

BASE_DECLARE_FEATURE(kTabOrganizationModelStrategy);

BASE_DECLARE_FEATURE(kTabOrganizationEnableNudgeForEnterprise);

// Duration of inactivity after which a tab is considered stale for declutter.
extern const base::FeatureParam<base::TimeDelta>
    kTabstripDeclutterStaleThresholdDuration;
// Interval between a recomputation of stale tabs for declutter.
extern const base::FeatureParam<base::TimeDelta>
    kTabstripDeclutterTimerInterval;
// Default interval after showing a nudge to prevent another nudge from being
// shown for declutter.
extern const base::FeatureParam<base::TimeDelta>
    kTabstripDeclutterNudgeTimerInterval;

// The target (and minimum) interval between proactive nudge triggers. Measured
// against a clock that only runs while Chrome is in the foreground.
extern const base::FeatureParam<base::TimeDelta> kTabOrganizationTriggerPeriod;

// The base to use for the trigger logic's exponential backoff.
extern const base::FeatureParam<double> kTabOrganizationTriggerBackoffBase;

// The minimum score threshold for proactive nudge triggering to occur.
extern const base::FeatureParam<double> kTabOrganizationTriggerThreshold;

// The maximum sensitivity score for a tab to contribute to trigger scoring.
extern const base::FeatureParam<double>
    kTabOrganizationTriggerSensitivityThreshold;

// Enable 'demo mode' for Tab Organization triggering, which triggers much more
// predictably and frequently.
extern const base::FeatureParam<bool> KTabOrganizationTriggerDemoMode;

BASE_DECLARE_FEATURE(kTabSearchChevronIcon);

BASE_DECLARE_FEATURE(kTabSearchFeedback);

BASE_DECLARE_FEATURE(kTabSearchRecentlyClosed);

// Default number of recently closed entries to display by default when no
// search text is provided.
extern const base::FeatureParam<int>
    kTabSearchRecentlyClosedDefaultItemDisplayCount;

// A threshold of recently closed tabs after which to stop adding recently
// closed item data to the profile data payload should the minimum display
// count have been met.
extern const base::FeatureParam<int> kTabSearchRecentlyClosedTabCountThreshold;

BASE_DECLARE_FEATURE(kTearOffWebAppTabOpensWebAppWindow);

BASE_DECLARE_FEATURE(kToolbarPinning);

bool IsToolbarPinningEnabled();

BASE_DECLARE_FEATURE(kEnterpriseProfileBadgingForAvatar);
BASE_DECLARE_FEATURE(kEnterpriseProfileBadgingForMenu);
BASE_DECLARE_FEATURE(kEnterpriseProfileBadgingPolicies);
BASE_DECLARE_FEATURE(kEnterpriseManagementDisclaimerUsesCustomLabel);
BASE_DECLARE_FEATURE(kEnterpriseUpdatedProfileCreationScreen);

BASE_DECLARE_FEATURE(kWebUITabStrip);

// Controls whether the context menu is shown on a touch press or a touch
// tap gesture on the WebUI Tab Strip.
BASE_DECLARE_FEATURE(kWebUITabStripContextMenuAfterTap);

// Cocoa to views migration.
#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kViewsFirstRunDialog);
BASE_DECLARE_FEATURE(kViewsJSAppModalDialog);
#endif

BASE_DECLARE_FEATURE(kStopLoadingAnimationForHiddenWindow);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kUsePortalAccentColor);
#endif

// This feature introduces a toggle that allows users to switch between the
// standard UI and a compact version of the UI by right clicking the empty area
// in the Tabstrip.
BASE_DECLARE_FEATURE(kCompactMode);

// Controls whether the site-specific data dialog shows a related installed
// applications section.
BASE_DECLARE_FEATURE(kPageSpecificDataDialogRelatedInstalledAppsSection);

}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
