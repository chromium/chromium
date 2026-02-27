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
#include "content/public/common/content_features.h"
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

BASE_FEATURE(kBrowserWidgetCacheThemeService,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCreateNewTabGroupAppMenuTopLevel,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDesktopGlowUp, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kGlassToolbar, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDetachedTabs, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kDseIntegrity, base::FEATURE_ENABLED_BY_DEFAULT);
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
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOfferPinToTaskbarInSettings, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Shows an infobar at startup offering to pin Chrome to the taskbar (on
// Windows) or the Dock (on MacOS).
BASE_FEATURE(kOfferPinToTaskbarInfoBar, base::FEATURE_ENABLED_BY_DEFAULT);
// Shows an infobar on PDFs offering to become the default PDF viewer if Chrome
// isn't the default already.
BASE_FEATURE(kPdfInfoBar, base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<PdfInfoBarTrigger>::Option
    kPdfInfoBarTriggerOptions[] = {{PdfInfoBarTrigger::kPdfLoad, "pdf-load"},
                                   {PdfInfoBarTrigger::kStartup, "startup"}};

BASE_FEATURE_ENUM_PARAM(PdfInfoBarTrigger,
                        kPdfInfoBarTrigger,
                        &kPdfInfoBar,
                        "trigger",
                        PdfInfoBarTrigger::kPdfLoad,
                        &kPdfInfoBarTriggerOptions);

BASE_FEATURE(kSeparateDefaultAndPinPrompt, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kSeparateDefaultAndPinPromptRandSeed,
                   &kSeparateDefaultAndPinPrompt,
                   "random_seed",
                   0);
BASE_FEATURE_PARAM(int,
                   kSeparateDefaultAndPinPromptPinMaxCount,
                   &kSeparateDefaultAndPinPrompt,
                   "pin_max_count",
                   5);
BASE_FEATURE_PARAM(int,
                   kSeparateDefaultAndPinPromptPinCooldownDays,
                   &kSeparateDefaultAndPinPrompt,
                   "pin_cooldown_days",
                   21);
BASE_FEATURE_PARAM(int,
                   kSeparateDefaultAndPinPromptDefaultMaxCount,
                   &kSeparateDefaultAndPinPrompt,
                   "default_max_count",
                   5);
BASE_FEATURE_PARAM(int,
                   kSeparateDefaultAndPinPromptDefaultCooldownDays,
                   &kSeparateDefaultAndPinPrompt,
                   "default_cooldown_days",
                   21);
BASE_FEATURE_PARAM(int,
                   kSeparateDefaultAndPinPromptMessageVersion,
                   &kSeparateDefaultAndPinPrompt,
                   "message_version",
                   0);
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

#if BUILDFLAG(IS_MAC)
// Add tab group colours when viewing tab groups using the top mac OS menu bar.
BASE_FEATURE(kShowTabGroupsMacSystemMenu, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

BASE_FEATURE(kSideBySide, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSideBySideLinkMenuNewBadge, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabDuplicateMetrics, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables tabs to be frozen when collapsed.
// https://crbug.com/1110108
BASE_FEATURE(kTabGroupsCollapseFreezing, base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
// General improvements to tab group menus
BASE_FEATURE(kTabGroupMenuImprovements, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupMenuImprovementsEnabled() {
  return base::FeatureList::IsEnabled(kTabGroupMenuImprovements);
}

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

BASE_FEATURE(kSidePanelFlyoverAnimation, base::FEATURE_ENABLED_BY_DEFAULT);
bool UseSidePanelFlyoverAnimation() {
#if BUILDFLAG(IS_MAC)
  // Mac can smoothly resize contents and does not need flyover.
  return false;
#else
  return base::FeatureList::IsEnabled(kSidePanelFlyoverAnimation);
#endif
}

// Enables enterprise profile badging for managed profiles on the toolbar avatar
// and in the profile menu. On managed profiles, a building icon will be used as
// a badge in the profile menu.
BASE_FEATURE(kEnterpriseProfileBadgingForMenu,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables enterprise badging for managed browsers on the new tab page footer.
// On managed browsers, a building icon and "Managed by <domain>" string will be
// shown in the footer, unless the icon and label are customized by the admin.
BASE_FEATURE(kEnterpriseBadgingForNtpFooter, base::FEATURE_ENABLED_BY_DEFAULT);

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
                   kPageActionsMigrationIntentPicker,
                   &kPageActionsMigration,
                   "intent_picker",
// TODOD(crbug.com/480035938): Enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
                   false
#else
                   true
#endif
);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationZoom,
                   &kPageActionsMigration,
                   "zoom",
                   false);

BASE_FEATURE_PARAM(bool,
                   kPageActionsMigrationFileSystemAccess,
                   &kPageActionsMigration,
                   "file_system_access",
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

bool IsWebUIHomeButtonEnabled() {
  return base::FeatureList::IsEnabled(features::kInitialWebUI) &&
         base::FeatureList::IsEnabled(features::kWebUIHomeButton);
}

bool IsWebUIBackForwardButtonEnabled() {
  return base::FeatureList::IsEnabled(features::kInitialWebUI) &&
         base::FeatureList::IsEnabled(features::kWebUIBackForwardButton);
}

bool IsWebUISplitTabsButtonEnabled() {
  return base::FeatureList::IsEnabled(features::kInitialWebUI) &&
         base::FeatureList::IsEnabled(features::kWebUISplitTabsButton);
}

bool IsWebUILocationBarEnabled() {
  return base::FeatureList::IsEnabled(features::kInitialWebUI) &&
         base::FeatureList::IsEnabled(features::kWebUILocationBar);
}

bool IsWebUIToolbarEnabled() {
  return IsWebUIReloadButtonEnabled() || IsWebUISplitTabsButtonEnabled() ||
         IsWebUIHomeButtonEnabled() || IsWebUILocationBarEnabled() ||
         IsWebUIBackForwardButtonEnabled();
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

BASE_FEATURE_PARAM(bool,
                   kTabGroupsFocusingPinnedTabs,
                   &kTabGroupsFocusing,
                   "tab_groups_focusing_pinned_tabs",
                   false);

BASE_FEATURE_PARAM(bool,
                   kTabGroupsFocusingDefaultToFocused,
                   &kTabGroupsFocusing,
                   "tab_groups_focusing_default_to_focused",
                   false);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kUpdaterUI, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace features
