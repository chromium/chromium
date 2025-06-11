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
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// TODO(crbug.com/40598679): Remove this when the tab dragging
// interactive_ui_tests pass on Wayland.
BASE_DECLARE_FEATURE(kAllowWindowDragUsingSystemDragDrop);

BASE_DECLARE_FEATURE(kAllowEyeDropperWGCScreenCapture);

BASE_DECLARE_FEATURE(kCloseOmniboxPopupOnInactiveAreaClick);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kFewerUpdateConfirmations);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Controls how extensions show up in the main menu. When enabled, if the
// current profile has no extensions, instead of a full extensions submenu, only
// the "Discover Chrome Extensions" item will be present.
BASE_DECLARE_FEATURE(kExtensionsCollapseMainMenu);

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kOfferPinToTaskbarWhenSettingToDefault);
BASE_DECLARE_FEATURE(kOfferPinToTaskbarInFirstRunExperience);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kPdfInfoBar);
enum class PdfInfoBarTrigger { kPdfLoad = 0, kStartup = 1 };
extern const base::FeatureParam<PdfInfoBarTrigger> kPdfInfoBarTrigger;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

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

// An list of exclude origins for WebUIs that don't participate in preloading.
// The list is a string of format "<origin>,<origin2>,...,<origin-n>", where
// each <origin> is a WebUI origin, e.g. "chrome://tab-search.top-chrome". This
// is used for emergency preloading shutoff for problematic WebUIs.
extern const char kPreloadTopChromeWebUIExcludeOriginsName[];
extern const base::FeatureParam<std::string>
    kPreloadTopChromeWebUIExcludeOrigins;

BASE_DECLARE_FEATURE(kPreloadTopChromeWebUILessNavigations);

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kPressAndHoldEscToExitBrowserFullscreen);
#endif

BASE_DECLARE_FEATURE(kScrimForBrowserWindowModal);

BASE_DECLARE_FEATURE(KScrimForTabModal);

BASE_DECLARE_FEATURE(kSideBySide);

BASE_DECLARE_FEATURE(kSideBySideLinkMenuNewBadge);

bool IsNtpFooterEnabledWithoutSideBySide();

BASE_DECLARE_FEATURE(kTabDuplicateMetrics);

BASE_DECLARE_FEATURE(kTabScrollingButtonPosition);
extern const char kTabScrollingButtonPositionParameterName[];

BASE_DECLARE_FEATURE(kSidePanelResizing);
BASE_DECLARE_FEATURE(kSidePanelSearchCompanion);

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

BASE_DECLARE_FEATURE(kTabstripDedupe);
bool IsTabstripDedupeEnabled();

BASE_DECLARE_FEATURE(kTabOrganizationAppMenuItem);

BASE_DECLARE_FEATURE(kTabOrganizationModelStrategy);

BASE_DECLARE_FEATURE(kTabOrganizationEnableNudgeForEnterprise);

BASE_DECLARE_FEATURE(kTabOrganizationUserInstruction);

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

BASE_DECLARE_FEATURE(kTearOffWebAppTabOpensWebAppWindow);

#if !BUILDFLAG(IS_ANDROID)
// Enables a three-button password save dialog variant (essentially adding a
// "not now" button alongside "never").
BASE_DECLARE_FEATURE(kThreeButtonPasswordSaveDialog);
#endif

bool IsToolbarPinningEnabled();

BASE_DECLARE_FEATURE(kPinnedCastButton);

BASE_DECLARE_FEATURE(kEnterpriseProfileBadgingForAvatar);
BASE_DECLARE_FEATURE(kEnterpriseProfileBadgingForMenu);
BASE_DECLARE_FEATURE(kEnterpriseBadgingForNtpFooter);
BASE_DECLARE_FEATURE(kNTPFooterBadgingPolicies);
BASE_DECLARE_FEATURE(kEnterpriseProfileBadgingPolicies);
BASE_DECLARE_FEATURE(kEnterpriseManagementDisclaimerUsesCustomLabel);
BASE_DECLARE_FEATURE(kEnterpriseUpdatedProfileCreationScreen);
BASE_DECLARE_FEATURE(kManagedProfileRequiredInterstitial);

// Enables using the same colors used for the default app menu button for the
// avatar button states using default colors.
BASE_DECLARE_FEATURE(kEnableAppMenuButtonColorsForDefaultAvatarButtonStates);

BASE_DECLARE_FEATURE(kWebUITabStrip);

// Controls whether the context menu is shown on a touch press or a touch
// tap gesture on the WebUI Tab Strip.
BASE_DECLARE_FEATURE(kWebUITabStripContextMenuAfterTap);

// Cocoa to views migration.
#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kViewsFirstRunDialog);
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

// Enable display for the Chrome Enterprise Core promotion banner on
// the chrome://policy page.
BASE_DECLARE_FEATURE(kEnablePolicyPromotionBanner);

// Controls whether a performance improvement in browser feature support
// checking is enabled.
BASE_DECLARE_FEATURE(kInlineFullscreenPerfExperiment);

// Controls whether the new page actions framework should be displaying page
// actions.
BASE_DECLARE_FEATURE(kPageActionsMigration);
// For development only, set this to enable all page actions.
extern const base::FeatureParam<bool> kPageActionsMigrationEnableAll;
// The following feature params indicate whether individual features should
// have their page actions controlled using the new framework.
extern const base::FeatureParam<bool> kPageActionsMigrationLensOverlay;
extern const base::FeatureParam<bool> kPageActionsMigrationMemorySaver;
extern const base::FeatureParam<bool> kPageActionsMigrationTranslate;
extern const base::FeatureParam<bool> kPageActionsMigrationIntentPicker;
extern const base::FeatureParam<bool> kPageActionsMigrationZoom;
extern const base::FeatureParam<bool> kPageActionsMigrationOfferNotification;
extern const base::FeatureParam<bool> kPageActionsMigrationFileSystemAccess;
extern const base::FeatureParam<bool> kPageActionsMigrationPwaInstall;
extern const base::FeatureParam<bool> kPageActionsMigrationPriceInsights;
extern const base::FeatureParam<bool> kPageActionsMigrationManagePasswords;

// Determines whether the "save password" page action displays different UI if
// the user has said to never save passwords for that site.
BASE_DECLARE_FEATURE(kSavePasswordsContextualUi);

// Controls whether browser tab loading animations are driven by the compositor
// vs. a repeating timer.
BASE_DECLARE_FEATURE(kCompositorLoadingAnimations);

// If enabled, the by date history will show in the side panel.
BASE_DECLARE_FEATURE(kByDateHistoryInSidePanel);

// Controls whether to use the TabStrip browser api's controller.
BASE_DECLARE_FEATURE(kTabStripBrowserApi);

// Controls where tab search lives in the browser.
BASE_DECLARE_FEATURE(kTabstripComboButton);
BASE_DECLARE_FEATURE(kLaunchedTabSearchToolbarButton);
extern const base::FeatureParam<bool> kTabstripComboButtonHasBackground;
extern const base::FeatureParam<bool> kTabstripComboButtonHasReverseButtonOrder;
extern const base::FeatureParam<bool> kTabSearchToolbarButton;
bool IsTabSearchMoving();
bool HasTabstripComboButtonWithBackground();
bool HasTabstripComboButtonWithReverseButtonOrder();
bool HasTabSearchToolbarButton();

}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
