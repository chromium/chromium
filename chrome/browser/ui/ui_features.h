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
BASE_DECLARE_FEATURE(kOfferPinToTaskbarInfoBar);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kPdfInfoBar);
enum class PdfInfoBarTrigger { kPdfLoad = 0, kStartup = 1 };
inline constexpr base::FeatureParam<PdfInfoBarTrigger>::Option
    kPdfInfoBarTriggerOptions[] = {{PdfInfoBarTrigger::kPdfLoad, "pdf-load"},
                                   {PdfInfoBarTrigger::kStartup, "startup"}};

inline constexpr base::FeatureParam<PdfInfoBarTrigger> kPdfInfoBarTrigger(
    &kPdfInfoBar,
    "trigger",
    PdfInfoBarTrigger::kPdfLoad,
    &kPdfInfoBarTriggerOptions);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

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
    kPreloadTopChromeWebUIMode(
        &kPreloadTopChromeWebUI,
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

BASE_DECLARE_FEATURE(kScrimForBrowserWindowModal);

BASE_DECLARE_FEATURE(KScrimForTabModal);

BASE_DECLARE_FEATURE(kSideBySide);

BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSideBySideShowDropTargetDelay);

BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetInnerPadding);

enum class MiniToolbarActiveConfiguration {
  // Hides the toolbar in the active view.
  Hide,
  // Shows only the menu button in the active view.
  ShowMenuOnly,
  // Shows favicon, domain, alerts and menu button in the active view.
  ShowAll
};

BASE_DECLARE_FEATURE_PARAM(MiniToolbarActiveConfiguration,
                           kSideBySideMiniToolbarActiveConfiguration);

BASE_DECLARE_FEATURE(kSideBySideLinkMenuNewBadge);

bool IsNtpFooterEnabledWithoutSideBySide();

BASE_DECLARE_FEATURE(kTabDuplicateMetrics);

BASE_DECLARE_FEATURE(kTabScrollingButtonPosition);

inline constexpr char kTabScrollingButtonPositionParameterName[] =
    "buttonPosition";

BASE_DECLARE_FEATURE(kSidePanelResizing);
BASE_DECLARE_FEATURE(kSidePanelSearchCompanion);

BASE_DECLARE_FEATURE(kTabGroupsCollapseFreezing);

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
inline constexpr base::FeatureParam<base::TimeDelta>
    kTabstripDeclutterStaleThresholdDuration(&kTabstripDeclutter,
                                             "stale_threshold_duration",
                                             base::Days(7));

// Interval between a recomputation of stale tabs for declutter.
inline constexpr base::FeatureParam<base::TimeDelta>
    kTabstripDeclutterTimerInterval(&kTabstripDeclutter,
                                    "declutter_timer_interval",
                                    base::Minutes(10));

// Default interval after showing a nudge to prevent another nudge from being
// shown for declutter.
inline constexpr base::FeatureParam<base::TimeDelta>
    kTabstripDeclutterNudgeTimerInterval(&kTabstripDeclutter,
                                         "nudge_timer_interval",
                                         base::Minutes(6 * 60));

// The target (and minimum) interval between proactive nudge triggers. Measured
// against a clock that only runs while Chrome is in the foreground.
inline constexpr base::FeatureParam<base::TimeDelta>
    kTabOrganizationTriggerPeriod(&kTabOrganization,
                                  "trigger_period",
                                  base::Hours(6));

// The base to use for the trigger logic's exponential backoff.
inline constexpr base::FeatureParam<double>
    kTabOrganizationTriggerBackoffBase(&kTabOrganization, "backoff_base", 2.0);

// The minimum score threshold for proactive nudge triggering to occur.
inline constexpr base::FeatureParam<double> kTabOrganizationTriggerThreshold(
    &kTabOrganization,
    "trigger_threshold",
    7.0);

// The maximum sensitivity score for a tab to contribute to trigger scoring.
inline constexpr base::FeatureParam<double>
    kTabOrganizationTriggerSensitivityThreshold(&kTabOrganization,
                                                "trigger_sensitivity_threshold",
                                                0.5);

// Enable 'demo mode' for Tab Organization triggering, which triggers much more
// predictably and frequently.
inline constexpr base::FeatureParam<bool> KTabOrganizationTriggerDemoMode(
    &kTabOrganization,
    "trigger_demo_mode",
    false);

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
inline constexpr base::FeatureParam<bool>
    kPageActionsMigrationEnableAll(&kPageActionsMigration, "enable_all", false);

// The following feature params indicate whether individual features should
// have their page actions controlled using the new framework.
inline constexpr base::FeatureParam<bool> kPageActionsMigrationLensOverlay(
    &kPageActionsMigration,
    "lens_overlay",
    false);

inline constexpr base::FeatureParam<bool> kPageActionsMigrationMemorySaver(
    &kPageActionsMigration,
    "memory_saver",
    false);

inline constexpr base::FeatureParam<bool>
    kPageActionsMigrationTranslate(&kPageActionsMigration, "translate", false);

inline constexpr base::FeatureParam<bool> kPageActionsMigrationIntentPicker(
    &kPageActionsMigration,
    "intent_picker",
    false);

inline constexpr base::FeatureParam<bool>
    kPageActionsMigrationZoom(&kPageActionsMigration, "zoom", false);

inline constexpr base::FeatureParam<bool>
    kPageActionsMigrationOfferNotification(&kPageActionsMigration,
                                           "offer_notification",
                                           false);

inline constexpr base::FeatureParam<bool> kPageActionsMigrationFileSystemAccess(
    &kPageActionsMigration,
    "file_system_access",
    false);

inline constexpr base::FeatureParam<bool> kPageActionsMigrationPwaInstall(
    &kPageActionsMigration,
    "pwa_install",
    false);

inline constexpr base::FeatureParam<bool> kPageActionsMigrationPriceInsights(
    &kPageActionsMigration,
    "price_insights",
    false);

inline constexpr base::FeatureParam<bool> kPageActionsMigrationManagePasswords(
    &kPageActionsMigration,
    "manage_passwords",
    false);

inline constexpr base::FeatureParam<bool> kPageActionsMigrationCookieControls(
    &kPageActionsMigration,
    "cookie_controls",
    false);

inline constexpr base::FeatureParam<bool> kPageActionsMigrationAutofillAddress(
    &kPageActionsMigration,
    "autofill_address",
    false);

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

inline constexpr base::FeatureParam<bool> kTabstripComboButtonHasBackground(
    &kTabstripComboButton,
    "has_background",
    false);

inline constexpr base::FeatureParam<bool>
    kTabstripComboButtonHasReverseButtonOrder(&kTabstripComboButton,
                                              "reverse_button_order",
                                              false);

inline constexpr base::FeatureParam<bool> kTabSearchToolbarButton(
    &kTabstripComboButton,
    "tab_search_toolbar_button",
    false);

bool IsTabSearchMoving();
bool HasTabstripComboButtonWithBackground();
bool HasTabstripComboButtonWithReverseButtonOrder();
bool HasTabSearchToolbarButton();

}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
