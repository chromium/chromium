// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "components/search/ntp_features.h"
#include "components/variations/service/variations_service.h"
#include "components/webui/flags/feature_entry.h"
#include "ui/base/ui_base_features.h"

namespace features {

// Enables the use of WGC for the Eye Dropper screen capture.
BASE_FEATURE(kAllowEyeDropperWGCScreenCapture,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_WIN)
);

// When enabled, clicks outside the omnibox and its popup will close an open
// omnibox popup.
BASE_FEATURE(kCloseOmniboxPopupOnInactiveAreaClick,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCreateNewTabGroupAppMenuTopLevel,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables the feature to remove the last confirmation dialog when relaunching
// to update Chrome.
BASE_FEATURE(kFewerUpdateConfirmations, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)

BASE_FEATURE(kExtensionsCollapseMainMenu, base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

BASE_FEATURE(kInfobarRefresh, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kOfferPinToTaskbarWhenSettingToDefault,
             "OfferPinToTaskbarWhenSettingDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOfferPinToTaskbarInFirstRunExperience,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOfferPinToTaskbarInSettings, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Shows an infobar at startup offering to pin Chrome to the taskbar (on
// Windows) or the Dock (on MacOS).
BASE_FEATURE(kOfferPinToTaskbarInfoBar, base::FEATURE_DISABLED_BY_DEFAULT);
// Shows an infobar on PDFs offering to become the default PDF viewer if Chrome
// isn't the default already.
BASE_FEATURE(kPdfInfoBar, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<PdfInfoBarTrigger>::Option
    kPdfInfoBarTriggerOptions[] = {{PdfInfoBarTrigger::kPdfLoad, "pdf-load"},
                                   {PdfInfoBarTrigger::kStartup, "startup"}};

BASE_FEATURE_ENUM_PARAM(PdfInfoBarTrigger,
                        kPdfInfoBarTrigger,
                        &kPdfInfoBar,
                        "trigger",
                        PdfInfoBarTrigger::kPdfLoad,
                        &kPdfInfoBarTriggerOptions);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Preloads a WebContents with a Top Chrome WebUI on BrowserView initialization,
// so that it can be shown instantly at a later time when necessary.
BASE_FEATURE(kPreloadTopChromeWebUI, base::FEATURE_ENABLED_BY_DEFAULT);

// An experiment to reduce the number of navigations when preloading WebUIs.
BASE_FEATURE(kPreloadTopChromeWebUILessNavigations,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables exiting browser fullscreen (users putting the browser itself into the
// fullscreen mode via the browser UI or shortcuts) with press-and-hold Esc.
BASE_FEATURE(kPressAndHoldEscToExitBrowserFullscreen,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, reloading using the toolbar button, hotkey, and web contents
// context menu will only reload the active tab. The tab context menu will still
// use the selection model to reload.
BASE_FEATURE(kReloadSelectionModel,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enforces close tab hotkey to only close the active view of a split tab,
// when it is the only tab in selection model.
BASE_FEATURE(kCloseActiveTabInSplitViewViaHotkey,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_MAC)
// Add tab group colours when viewing tab groups using the top mac OS menu bar.
BASE_FEATURE(kShowTabGroupsMacSystemMenu, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

BASE_FEATURE(kSideBySide,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// The delay before showing the drop target for the side-by-side drag-and-drop
// entrypoint.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSideBySideShowDropTargetDelay,
                   &kSideBySide,
                   "drop_target_show_delay",
                   base::Milliseconds(500));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSideBySideShowDropTargetForLinkDelay,
                   &kSideBySide,
                   "drop_target_for_link_show_delay",
                   base::Milliseconds(500));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSideBySideShowDropTargetForLinkAfterHideDelay,
                   &kSideBySide,
                   "drop_target_for_link_after_hide_show_delay",
                   base::Milliseconds(3000));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSideBySideShowDropTargetForLinkAfterHideLookbackWindow,
                   &kSideBySide,
                   "drop_target_for_link_after_hide_lookback_window",
                   base::Seconds(30));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSideBySideHideDropTargetDelay,
                   &kSideBySide,
                   "drop_target_hide_delay",
                   base::Milliseconds(100));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSideBySideShowNudgeDelay,
                   &kSideBySide,
                   "show_nudge_delay",
                   base::Milliseconds(1000));
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetMinWidth,
                   &kSideBySide,
                   "drop_target_min_width",
                   120);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetMaxWidth,
                   &kSideBySide,
                   "drop_target_max_width",
                   360);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetTargetWidthPercentage,
                   &kSideBySide,
                   "drop_target_width_percentage",
                   15);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetForLinkTargetWidthPercentage,
                   &kSideBySide,
                   "drop_target_for_link_width_percentage",
                   15);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetHideForOSWidth,
                   &kSideBySide,
                   "drop_target_hide_for_os_width",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
                   32
#elif BUILDFLAG(IS_LINUX)
                   50
#else
                   0
#endif
);

BASE_FEATURE_PARAM(double,
                   kSideBySideDropTargetHideForOSPercentage,
                   &kSideBySide,
                   "drop_target_hide_for_os_percentage",
#if BUILDFLAG(IS_WIN)
                   1.4
#else
                   0
#endif
);

BASE_FEATURE(kSideBySideDropTargetNudge,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetNudgeMinWidth,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_min_width",
                   80);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetNudgeMaxWidth,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_max_width",
                   200);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetNudgeTargetWidthPercentage,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_width_percentage",
                   5);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetNudgeToFullMinWidth,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_to_full_min_width",
                   120);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetNudgeToFullMaxWidth,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_to_full_max_width",
                   360);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetNudgeToFullTargetWidthPercentage,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_to_full_width_percentage",
                   15);
BASE_FEATURE_PARAM(double,
                   kSideBySideDropTargetNudgeShowRatio,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_show_ratio",
                   0.4f);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetNudgeShownLimit,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_shown_limit",
                   6);
BASE_FEATURE_PARAM(int,
                   kSideBySideDropTargetNudgeUsedLimit,
                   &kSideBySideDropTargetNudge,
                   "drop_target_nudge_used_limit",
                   1);

constexpr base::FeatureParam<MiniToolbarActiveConfiguration>::Option
    kMiniToolbarActiveConfigurationOptions[] = {
        {MiniToolbarActiveConfiguration::Hide, "hide"},
        {MiniToolbarActiveConfiguration::ShowMenu, "showmenu"},
        {MiniToolbarActiveConfiguration::ShowClose, "showclose"}};

// The active configuration for the mini toolbar on active view of a split.
BASE_FEATURE_ENUM_PARAM(MiniToolbarActiveConfiguration,
                        kSideBySideMiniToolbarActiveConfiguration,
                        &kSideBySide,
                        "mini_toolbar_active_config",
                        MiniToolbarActiveConfiguration::ShowMenu,
                        &kMiniToolbarActiveConfigurationOptions);

BASE_FEATURE_PARAM(int,
                   kSideBySideSnapDistance,
                   &kSideBySide,
                   "snap_distance",
                   15);

BASE_FEATURE_PARAM(int,
                   kSideBySideIphTabSwitchCount,
                   &kSideBySide,
                   "side_by_side_iph_tab_switch_count",
                   3);

// When enabled along with SideBySide flag, split tabs will be restored on
// startup.
BASE_FEATURE(kSideBySideSessionRestore,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsRestoringSplitViewEnabled() {
  return base::FeatureList::IsEnabled(features::kSideBySide) &&
         base::FeatureList::IsEnabled(features::kSideBySideSessionRestore);
}

BASE_FEATURE(kSideBySideLinkMenuNewBadge,

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSideBySideKeyboardShortcut, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSideBySideKeyboardShortcutEnabled() {
  return base::FeatureList::IsEnabled(features::kSideBySide) &&
         base::FeatureList::IsEnabled(features::kSideBySideKeyboardShortcut);
}

BASE_FEATURE(kSideBySideFocusClearing, base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<SidePanelRelativeAlignment>::Option
    kSidePanelRelativeAlignmentOptions[] = {
        {SidePanelRelativeAlignment::kShowPanelsOnSameSide, "same"},
        {SidePanelRelativeAlignment::kShowPanelsOnOppositeSides, "opposite"}};

BASE_FEATURE_ENUM_PARAM(SidePanelRelativeAlignment,
                        kSidePanelRelativeAlignment,
                        &kToolbarHeightSidePanel,
                        "side_panel_relative_alignment",
                        SidePanelRelativeAlignment::kShowPanelsOnOppositeSides,
                        &kSidePanelRelativeAlignmentOptions);

BASE_FEATURE(kAppBrowserUseNewLayout, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPopupBrowserUseNewLayout, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabbedBrowserUseNewLayout, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabDuplicateMetrics, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables buttons when scrolling the tabstrip https://crbug.com/951078
BASE_FEATURE(kTabScrollingButtonPosition, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables tabs to be frozen when collapsed.
// https://crbug.com/1110108
BASE_FEATURE(kTabGroupsCollapseFreezing, base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
// General improvements to tab group menus
BASE_FEATURE(kTabGroupMenuImprovements, base::FEATURE_DISABLED_BY_DEFAULT);

// Update menus to use tab group menus in the action menu
BASE_FEATURE(kTabGroupMenuMoreEntryPoints, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupMenuMoreEntryPointsEnabled() {
  return base::FeatureList::IsEnabled(kTabGroupMenuMoreEntryPoints);
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Enables preview images in tab-hover cards.
// https://crbug.com/928954
BASE_FEATURE(kTabHoverCardImages,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kTabGroupHoverCards, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabModalUsesDesktopWidget, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganization, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabOrganization() {
  return base::FeatureList::IsEnabled(features::kTabOrganization);
}

BASE_FEATURE_PARAM(base::TimeDelta,
                   kTabOrganizationTriggerPeriod,
                   &kTabOrganization,
                   "trigger_period",
                   base::Hours(6));

BASE_FEATURE_PARAM(double,
                   kTabOrganizationTriggerBackoffBase,
                   &kTabOrganization,
                   "backoff_base",
                   2.0);

BASE_FEATURE_PARAM(double,
                   kTabOrganizationTriggerThreshold,
                   &kTabOrganization,
                   "trigger_threshold",
                   7.0);

BASE_FEATURE_PARAM(double,
                   kTabOrganizationTriggerSensitivityThreshold,
                   &kTabOrganization,
                   "trigger_sensitivity_threshold",
                   0.5);

BASE_FEATURE_PARAM(bool,
                   KTabOrganizationTriggerDemoMode,
                   &kTabOrganization,
                   "trigger_demo_mode",
                   false);

BASE_FEATURE(kTabstripDeclutter, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabstripDeclutterEnabled() {
  return base::FeatureList::IsEnabled(features::kTabstripDeclutter);
}

BASE_FEATURE_PARAM(base::TimeDelta,
                   kTabstripDeclutterStaleThresholdDuration,
                   &kTabstripDeclutter,
                   "stale_threshold_duration",
                   base::Days(7));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kTabstripDeclutterTimerInterval,
                   &kTabstripDeclutter,
                   "declutter_timer_interval",
                   base::Minutes(10));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kTabstripDeclutterNudgeTimerInterval,
                   &kTabstripDeclutter,
                   "nudge_timer_interval",
                   base::Minutes(6 * 60));

BASE_FEATURE(kTabstripDedupe, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabstripDedupeEnabled() {
  return IsTabstripDeclutterEnabled() &&
         base::FeatureList::IsEnabled(features::kTabstripDedupe);
}

BASE_FEATURE(kTabOrganizationAppMenuItem, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationModelStrategy, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationUserInstruction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationEnableNudgeForEnterprise,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables creating a web app window when tearing off a tab with a url
// controlled by a web app.
BASE_FEATURE(kTearOffWebAppTabOpensWebAppWindow,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kThreeButtonPasswordSaveDialog, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kToolbarHeightSidePanel, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables enterprise profile badging for managed profiles on the toolbar avatar
// and in the profile menu. On managed profiles, a building icon will be used as
// a badge in the profile menu.
BASE_FEATURE(kEnterpriseProfileBadgingForMenu,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables enterprise badging for managed browsers on the new tab page footer.
// On managed browsers, a building icon and "Managed by <domain>" string will be
// shown in the footer, unless the icon and label are customized by the admin.
BASE_FEATURE(kEnterpriseBadgingForNtpFooter, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables enterprise badging for managed browsers with local management only on
// the new tab page footer. On managed browsers, a building icon and "Managed by
// your organization" string will be shown in the footer.
BASE_FEATURE(kEnterpriseBadgingForLocalManagemenetNtpFooter,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables enterprise badging for managed browsers with local management only
// AND 3 or more policies on the new tab page footer.
BASE_FEATURE(kEnterpriseBadgingForNtpFooterWithOverThreePolicies,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the management notice in the NTP footer if the custom policies are
// set. This acts as a kill switch for "EnterpriseCustomLabelForBrowser" and
// "EnterpriseLogoUrlForBrowser".
BASE_FEATURE(kNTPFooterBadgingPolicies, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing the EnterpriseCustomLabel` instead of the cloud policy
// manager in the managed disclaimer "Managed by..." in the profile and app
// menus.
BASE_FEATURE(kEnterpriseManagementDisclaimerUsesCustomLabel,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kManagedProfileRequiredInterstitial,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a web-based tab strip. See https://crbug.com/989131. Note this
// feature only works when the ENABLE_WEBUI_TAB_STRIP buildflag is enabled.
BASE_FEATURE(kWebUITabStrip,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// The default value of this flag is aligned with platform behavior to handle
// context menu with touch.
// TODO(crbug.com/40796475): Enable this flag for all platforms after launch.
BASE_FEATURE(kWebUITabStripContextMenuAfterTap,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kViewsFirstRunDialog, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kViewsJSAppModalDialog, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kUsePortalAccentColor, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPageSpecificDataDialogRelatedInstalledAppsSection,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableManagementPromotionBanner,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kEnablePolicyPromotionBanner, base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kEnablePolicyPromotionBanner, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kInlineFullscreenPerfExperiment, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageActionsMigration, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationEnableAll,
                   &kPageActionsMigration,
                   "enable_all",
                   false);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationLensOverlay,
                   &kPageActionsMigration,
                   "lens_overlay",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationMemorySaver,
                   &kPageActionsMigration,
                   "memory_saver",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationTranslate,
                   &kPageActionsMigration,
                   "translate",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationIntentPicker,
                   &kPageActionsMigration,
                   "intent_picker",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationZoom,
                   &kPageActionsMigration,
                   "zoom",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationOfferNotification,
                   &kPageActionsMigration,
                   "offer_notification",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationFileSystemAccess,
                   &kPageActionsMigration,
                   "file_system_access",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationPwaInstall,
                   &kPageActionsMigration,
                   "pwa_install",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationPriceInsights,
                   &kPageActionsMigration,
                   "price_insights",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationDiscounts,
                   &kPageActionsMigration,
                   "discounts",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationManagePasswords,
                   &kPageActionsMigration,
                   "manage_passwords",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationCookieControls,
                   &kPageActionsMigration,
                   "cookie_controls",
                   false);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationAutofillAddress,
                   &kPageActionsMigration,
                   "autofill_address",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationFind,
                   &kPageActionsMigration,
                   "find",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationCollaborationMessaging,
                   &kPageActionsMigration,
                   "collaboration_messaging",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationPriceTracking,
                   &kPageActionsMigration,
                   "price_tracking",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationAutofillMandatoryReauth,
                   &kPageActionsMigration,
                   "mandatory_reauth",
                   false);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationClickToCall,
                   &kPageActionsMigration,
                   "click_to_call",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationSharingHub,
                   &kPageActionsMigration,
                   "sharing_hub",
                   false);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationAiMode,
                   &kPageActionsMigration,
                   "ai_mode",
                   false);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationVirtualCard,
                   &kPageActionsMigration,
                   "virtual_card",
                   false);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationFilledCardInformation,
                   &kPageActionsMigration,
                   "filled_card_information",
                   false);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationReadingMode,
                   &kPageActionsMigration,
                   "reading_mode",
                   true);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationSavePayments,
                   &kPageActionsMigration,
                   "save_payments",
                   false);
BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationLensOverlayHomework,
                   &kPageActionsMigration,
                   "lens_overlay_homework",
                   false);
BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationBookmarkStar,
                   &kPageActionsMigration,
                   "bookmark_star",
                   false);

BASE_FEATURE(kSavePasswordsContextualUi, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kByDateHistoryInSidePanel, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripBrowserApi, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabstripComboButton, base::FEATURE_DISABLED_BY_DEFAULT);

// This serves as a "kill-switch" for migrating the Tab Search feature to be a
// toolbar button for non-ChromeOS users in the US.
BASE_FEATURE(kLaunchedTabSearchToolbarButton,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(bool,
                   kTabSearchToolbarButton,
                   &kTabstripComboButton,
                   "tab_search_toolbar_button",
                   true);

static std::string GetCountryCode() {
  if (!g_browser_process || !g_browser_process->variations_service()) {
    return std::string();
  }
  std::string country_code =
      g_browser_process->variations_service()->GetStoredPermanentCountry();
  if (country_code.empty()) {
    country_code = g_browser_process->variations_service()->GetLatestCountry();
  }
  return country_code;
}

bool HasTabSearchToolbarButton() {
  static const bool is_tab_search_moving = [] {
    if (GetCountryCode() == "us" &&
        base::FeatureList::IsEnabled(
            features::kLaunchedTabSearchToolbarButton)) {
      return true;
    }
    return base::FeatureList::IsEnabled(features::kTabstripComboButton) &&
           features::kTabSearchToolbarButton.Get();
  }();

  return is_tab_search_moving;
}

BASE_FEATURE(kNonMilestoneUpdateToast, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBookmarkTabGroupConversion, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBookmarkTabGroupConversionEnabled() {
  return base::FeatureList::IsEnabled(kBookmarkTabGroupConversion);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kSessionRestoreInfobar, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kSetDefaultToContinueSession,
                   &kSessionRestoreInfobar,
                   "continue_session",
                   false);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kNewTabAddsToActiveGroup, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewTabAddsToActiveGroupEnabled() {
  return base::FeatureList::IsEnabled(kNewTabAddsToActiveGroup);
}

bool IsWebUIReloadButtonEnabled() {
  return base::FeatureList::IsEnabled(features::kInitialWebUI) &&
         base::FeatureList::IsEnabled(features::kWebUIReloadButton);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAndroidAnimatedProgressBarInBrowser,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAndroidAnimatedProgressBarInBrowserEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAndroidAnimatedProgressBarInBrowser);
}
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kWhatsNewDesktopRefresh, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsFocusing, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
