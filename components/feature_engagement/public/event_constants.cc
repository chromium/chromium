// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/event_constants.h"

#include "build/build_config.h"

namespace feature_engagement {

namespace events {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
const char kNewTabOpened[] = "new_tab_opened";
const char kSixthTabOpened[] = "sixth_tab_opened";
const char kTabGroupCreated[] = "tab_group_created";
const char kClosedTabWithEightOrMore[] = "closed_tab_with_eight_or_more";
const char kReadingListItemAdded[] = "reading_list_item_added";
const char kReadingListMenuOpened[] = "reading_list_menu_opened";
const char kBookmarkStarMenuOpened[] = "bookmark_star_menu_opened";
const char kCustomizeChromeOpened[] = "customize_chrome_opened";

const char kReopenTabConditionsMet[] = "reopen_tab_conditions_met";
const char kTabReopened[] = "tab_reopened";

const char kMediaBackgrounded[] = "media_backgrounded";
const char kGlobalMediaControlsOpened[] = "global_media_controls_opened";

const char kFocusModeOpened[] = "focus_mode_opened";
const char kFocusModeConditionsMet[] = "focus_mode_conditions_met";

const char kSideSearchAutoTriggered[] = "side_search_auto_triggered";
const char kSideSearchOpened[] = "side_search_opened";
const char kSideSearchPageActionLabelShown[] =
    "side_search_page_action_label_shown";

const char kTabSearchOpened[] = "tab_search_opened";

const char kWebUITabStripClosed[] = "webui_tab_strip_closed";
const char kWebUITabStripOpened[] = "webui_tab_strip_opened";

const char kDesktopPwaInstalled[] = "desktop_pwa_installed";

const char kFocusHelpBubbleAcceleratorPressed[] =
    "focus_help_bubble_accelerator_pressed";

const char kFocusHelpBubbleAcceleratorPromoRead[] =
    "focus_help_bubble_accelerator_promo_read";

const char kBatterySaverDialogShown[] = "battery_saver_info_shown";

const char kHighEfficiencyDialogShown[] = "high_efficiency_info_shown";

const char kPerformanceMenuItemActivated[] = "performance_activated";

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_IOS)
const char kChromeOpened[] = "chrome_opened";
const char kIncognitoTabOpened[] = "incognito_tab_opened";
const char kClearedBrowsingData[] = "cleared_browsing_data";
const char kViewedReadingList[] = "viewed_reading_list";
const char kViewedWhatsNew[] = "viewed_whats_new";
const char kTriggeredTranslateInfobar[] = "triggered_translate_infobar";
const char kBottomToolbarOpened[] = "bottom_toolbar_opened";
const char kDiscoverFeedLoaded[] = "discover_feed_loaded";
const char kDesktopVersionRequested[] = "desktop_version_requested";
const char kDefaultSiteViewUsed[] = "default_site_view_used";
const char kOverflowMenuNoHorizontalScrollOrAction[] =
    "overflow_menu_no_horizontal_scroll_or_action";
const char kPriceNotificationsUsed[] = "price_notifications_used";
const char kDefaultBrowserPromoShown[] = "default_browser_promo_shown";

// Default browser blue dot promo.
const char kBlueDotPromoCriterionMet[] = "blue_dot_promo_criterion_met";
const char kBlueDotPromoEligibilityMet[] = "blue_dot_promo_eligibility_met";
const char kBlueDotPromoOverflowMenuShown[] =
    "blue_dot_promo_overflow_menu_shown";
const char kBlueDotPromoOverflowMenuShownNewSession[] =
    "blue_dot_promo_overflow_menu_shown_new_session";
const char kBlueDotPromoSettingsShown[] = "blue_dot_promo_settings_shown";
const char kBlueDotPromoSettingsShownNewSession[] =
    "blue_dot_promo_settings_shown_new_session";
const char kBlueDotPromoOverflowMenuDismissed[] =
    "blue_dot_promo_overflow_menu_dismissed";
const char kBlueDotPromoSettingsDismissed[] =
    "blue_dot_promo_settings_dismissed";
const char kCredentialProviderExtensionPromoSnoozed[] =
    "credential_provider_extension_promo_snoozed";
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
const char kPwaInstallMenuSelected[] = "pwa_install_menu_clicked";
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace events

}  // namespace feature_engagement
