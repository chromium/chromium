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

BASE_DECLARE_FEATURE(kAllowEyeDropperWGCScreenCapture);

BASE_DECLARE_FEATURE(kCreateNewTabGroupAppMenuTopLevel);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kFewerUpdateConfirmations);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Controls how extensions show up in the main menu. When enabled, if the
// current profile has no extensions, instead of a full extensions submenu, only
// the "Discover Chrome Extensions" item will be present.
BASE_DECLARE_FEATURE(kExtensionsCollapseMainMenu);

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Controls whether the refreshed infobar is enabled.
BASE_DECLARE_FEATURE(kInfobarRefresh);

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kOfferPinToTaskbarWhenSettingToDefault);
BASE_DECLARE_FEATURE(kOfferPinToTaskbarInFirstRunExperience);
BASE_DECLARE_FEATURE(kOfferPinToTaskbarInSettings);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kOfferPinToTaskbarInfoBar);
BASE_DECLARE_FEATURE(kPdfInfoBar);

enum class PdfInfoBarTrigger { kPdfLoad = 0, kStartup = 1 };

BASE_DECLARE_FEATURE_PARAM(PdfInfoBarTrigger, kPdfInfoBarTrigger);
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

BASE_DECLARE_FEATURE(kReloadSelectionModel);

BASE_DECLARE_FEATURE(kCloseActiveTabInSplitViewViaHotkey);

BASE_DECLARE_FEATURE(kScrimForBrowserWindowModal);

BASE_DECLARE_FEATURE(kSideBySide);

BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSideBySideShowDropTargetDelay);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSideBySideShowDropTargetForLinkDelay);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSideBySideShowDropTargetForLinkAfterHideDelay);
BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kSideBySideShowDropTargetForLinkAfterHideLookbackWindow);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSideBySideHideDropTargetDelay);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSideBySideShowNudgeDelay);

// Feature params for the width of the multi-contents drop target.
// If the `kSideBySideDropTargetNudge` feature is enabled, then these only
// apply for tab dragging.
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetMinWidth);
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetMaxWidth);
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetTargetWidthPercentage);
BASE_DECLARE_FEATURE_PARAM(int,
                           kSideBySideDropTargetForLinkTargetWidthPercentage);

// The size of the edge of the screen where the Split View drop target is hidden
// will be the max of the width and the percentage times the screen width.
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetHideForOSWidth);
BASE_DECLARE_FEATURE_PARAM(double, kSideBySideDropTargetHideForOSPercentage);

// Feature and params to control the "nudge" behavior of drop targets.
BASE_DECLARE_FEATURE(kSideBySideDropTargetNudge);
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetNudgeMinWidth);
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetNudgeMaxWidth);
BASE_DECLARE_FEATURE_PARAM(int,
                           kSideBySideDropTargetNudgeTargetWidthPercentage);
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetNudgeToFullMinWidth);
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetNudgeToFullMaxWidth);
BASE_DECLARE_FEATURE_PARAM(
    int,
    kSideBySideDropTargetNudgeToFullTargetWidthPercentage);
// The ratio of window width that will trigger a nudge to show/hide.
BASE_DECLARE_FEATURE_PARAM(double, kSideBySideDropTargetNudgeShowRatio);
// The total amount of times the nudge may be shown before we stop showing it.
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetNudgeShownLimit);
// The total amount of times the drop target may be used with a link before we
// stop showing the nudge.
BASE_DECLARE_FEATURE_PARAM(int, kSideBySideDropTargetNudgeUsedLimit);

enum class MiniToolbarActiveConfiguration {
  // Hides the toolbar in the active view.
  Hide,
  // Shows only the menu button in the active view.
  ShowMenu,
  // Shows only the close button in the active view.
  ShowClose,
};

BASE_DECLARE_FEATURE_PARAM(MiniToolbarActiveConfiguration,
                           kSideBySideMiniToolbarActiveConfiguration);

BASE_DECLARE_FEATURE_PARAM(int, kSideBySideSnapDistance);

BASE_DECLARE_FEATURE_PARAM(int, kSideBySideIphTabSwitchCount);

BASE_DECLARE_FEATURE(kSideBySideSessionRestore);

bool IsRestoringSplitViewEnabled();

BASE_DECLARE_FEATURE(kSideBySideLinkMenuNewBadge);

BASE_DECLARE_FEATURE(kSideBySideKeyboardShortcut);

bool IsSideBySideKeyboardShortcutEnabled();

BASE_DECLARE_FEATURE(kSideBySideFocusClearing);

enum class SidePanelRelativeAlignment {
  // Shows the toolbar and content height side panels on the same side.
  kShowPanelsOnSameSide,
  // Shows the toolbar and content height side panels on opposite sides.
  kShowPanelsOnOppositeSides,
};
BASE_DECLARE_FEATURE_PARAM(SidePanelRelativeAlignment,
                           kSidePanelRelativeAlignment);

BASE_DECLARE_FEATURE(kAppBrowserUseNewLayout);

BASE_DECLARE_FEATURE(kPopupBrowserUseNewLayout);

BASE_DECLARE_FEATURE(kTabbedBrowserUseNewLayout);

BASE_DECLARE_FEATURE(kTabDuplicateMetrics);

BASE_DECLARE_FEATURE(kTabScrollingButtonPosition);

inline constexpr char kTabScrollingButtonPositionParameterName[] =
    "buttonPosition";

BASE_DECLARE_FEATURE(kTabGroupsCollapseFreezing);
BASE_DECLARE_FEATURE(kTabGroupHoverCards);

#if !BUILDFLAG(IS_ANDROID)
// General improvements to tab group menus
BASE_DECLARE_FEATURE(kTabGroupMenuImprovements);
BASE_DECLARE_FEATURE(kTabGroupMenuMoreEntryPoints);
bool IsTabGroupMenuMoreEntryPointsEnabled();

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

BASE_DECLARE_FEATURE(kTabOrganization);
bool IsTabOrganization();

// The target (and minimum) interval between proactive nudge triggers. Measured
// against a clock that only runs while Chrome is in the foreground.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kTabOrganizationTriggerPeriod);

// The base to use for the trigger logic's exponential backoff.
BASE_DECLARE_FEATURE_PARAM(double, kTabOrganizationTriggerBackoffBase);

// The minimum score threshold for proactive nudge triggering to occur.
BASE_DECLARE_FEATURE_PARAM(double, kTabOrganizationTriggerThreshold);

// The maximum sensitivity score for a tab to contribute to trigger scoring.
BASE_DECLARE_FEATURE_PARAM(double, kTabOrganizationTriggerSensitivityThreshold);

// Enable 'demo mode' for Tab Organization triggering, which triggers much more
// predictably and frequently.
BASE_DECLARE_FEATURE_PARAM(bool, KTabOrganizationTriggerDemoMode);

BASE_DECLARE_FEATURE(kTabstripDeclutter);
bool IsTabstripDeclutterEnabled();

// Duration of inactivity after which a tab is considered stale for declutter.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kTabstripDeclutterStaleThresholdDuration);

// Interval between a recomputation of stale tabs for declutter.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kTabstripDeclutterTimerInterval);

// Default interval after showing a nudge to prevent another nudge from being
// shown for declutter.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kTabstripDeclutterNudgeTimerInterval);

BASE_DECLARE_FEATURE(kTabstripDedupe);
bool IsTabstripDedupeEnabled();

BASE_DECLARE_FEATURE(kTabOrganizationAppMenuItem);

BASE_DECLARE_FEATURE(kTabOrganizationModelStrategy);

BASE_DECLARE_FEATURE(kTabOrganizationEnableNudgeForEnterprise);

BASE_DECLARE_FEATURE(kTabOrganizationUserInstruction);

BASE_DECLARE_FEATURE(kTearOffWebAppTabOpensWebAppWindow);

#if !BUILDFLAG(IS_ANDROID)
// Enables a three-button password save dialog variant (essentially adding a
// "not now" button alongside "never").
BASE_DECLARE_FEATURE(kThreeButtonPasswordSaveDialog);
#endif

// Enables a side panel that occupies the vertical space from the top of the
// toolbar to the bottom of the browser. This is taller than the default side
// panel, which occupies the space from the top of the WebContents to the bottom
// of the browser.
BASE_DECLARE_FEATURE(kToolbarHeightSidePanel);

// TODO(crbug.com/460764864): Cleanup all the enterprise badging feature flags.
BASE_DECLARE_FEATURE(kEnterpriseProfileBadgingForMenu);
BASE_DECLARE_FEATURE(kEnterpriseBadgingForNtpFooter);
BASE_DECLARE_FEATURE(kEnterpriseBadgingForLocalManagemenetNtpFooter);
BASE_DECLARE_FEATURE(kEnterpriseBadgingForNtpFooterWithOverThreePolicies);
BASE_DECLARE_FEATURE(kNTPFooterBadgingPolicies);

BASE_DECLARE_FEATURE(kEnterpriseManagementDisclaimerUsesCustomLabel);
BASE_DECLARE_FEATURE(kManagedProfileRequiredInterstitial);

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
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationEnableAll);

// The following feature params indicate whether individual features should
// have their page actions controlled using the new framework.
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationLensOverlay);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationMemorySaver);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationTranslate);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationIntentPicker);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationZoom);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationOfferNotification);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationFileSystemAccess);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationPwaInstall);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationPriceInsights);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationDiscounts);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationManagePasswords);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationCookieControls);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationAutofillAddress);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationFind);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationCollaborationMessaging);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationPriceTracking);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationAutofillMandatoryReauth);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationClickToCall);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationSharingHub);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationAiMode);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationVirtualCard);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationFilledCardInformation);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationReadingMode);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationSavePayments);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationLensOverlayHomework);
BASE_DECLARE_FEATURE_PARAM(bool, kPageActionsMigrationBookmarkStar);

// Determines whether the "save password" page action displays different UI if
// the user has said to never save passwords for that site.
BASE_DECLARE_FEATURE(kSavePasswordsContextualUi);

#if BUILDFLAG(IS_MAC)
// Add tab group colours when viewing tab groups using the top mac OS menu bar.
BASE_DECLARE_FEATURE(kShowTabGroupsMacSystemMenu);
#endif  // BUILDFLAG(IS_MAC)

// If enabled, the by date history will show in the side panel.
BASE_DECLARE_FEATURE(kByDateHistoryInSidePanel);

// Controls whether to use the TabStrip browser api's controller.
BASE_DECLARE_FEATURE(kTabStripBrowserApi);

// Controls where tab search lives in the browser. By default, the tab search
// feature lives in the tab strip. The feature moves to the toolbar button if
// the user is in the US and `kLaunchedTabSearchToolbarButton` is enabled or if
// `kTabstripComboButton` is enabled and `kTabSearchToolbarButton` is true.
BASE_DECLARE_FEATURE(kTabstripComboButton);
BASE_DECLARE_FEATURE(kLaunchedTabSearchToolbarButton);

BASE_DECLARE_FEATURE_PARAM(bool, kTabSearchToolbarButton);

bool HasTabSearchToolbarButton();

#if !BUILDFLAG(IS_ANDROID)
// Controls whether to add new tabs to active tab group or to the end of the
// tab strip.
BASE_DECLARE_FEATURE(kNewTabAddsToActiveGroup);

bool IsNewTabAddsToActiveGroupEnabled();

bool IsWebUIReloadButtonEnabled();
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

// Controls whether the updated What's New page is enabled.
BASE_DECLARE_FEATURE(kWhatsNewDesktopRefresh);

BASE_DECLARE_FEATURE(kTabGroupsFocusing);

}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
