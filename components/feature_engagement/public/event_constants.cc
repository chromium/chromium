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
const char kReadingListItemAdded[] = "reading_list_item_added";
const char kReadingListMenuOpened[] = "reading_list_menu_opened";
const char kBookmarkStarMenuOpened[] = "bookmark_star_menu_opened";
const char kCustomizeChromeOpened[] = "customize_chrome_opened";

const char kMediaBackgrounded[] = "media_backgrounded";
const char kGlobalMediaControlsOpened[] = "global_media_controls_opened";

const char kSidePanelPinned[] = "side_panel_pinned";

const char kSideSearchAutoTriggered[] = "side_search_auto_triggered";
const char kSideSearchOpened[] = "side_search_opened";
const char kSideSearchPageActionLabelShown[] =
    "side_search_page_action_label_shown";

const char kTabSearchOpened[] = "tab_search_opened";

const char kDesktopNTPModuleUsed[] = "desktop_new_tab_page_modules_used";

const char kDesktopPwaInstalled[] = "desktop_pwa_installed";

const char kFocusHelpBubbleAcceleratorPressed[] =
    "focus_help_bubble_accelerator_pressed";

const char kFocusHelpBubbleAcceleratorPromoRead[] =
    "focus_help_bubble_accelerator_promo_read";

const char kExtensionsRequestAccessButtonClicked[] =
    "extensions_request_access_button_clicked";

const char kCookieControlsBubbleShown[] = "cookie_controls_bubble_shown";

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_IOS)
const char kChromeOpened[] = "chrome_opened";
const char kIncognitoTabOpened[] = "incognito_tab_opened";
const char kClearedBrowsingData[] = "cleared_browsing_data";
const char kViewedReadingList[] = "viewed_reading_list";
const char kViewedWhatsNew[] = "viewed_whats_new_m128";
const char kTriggeredTranslateInfobar[] = "triggered_translate_infobar";
const char kBottomToolbarOpened[] = "bottom_toolbar_opened";
const char kDiscoverFeedLoaded[] = "discover_feed_loaded";
const char kDesktopVersionRequested[] = "desktop_version_requested";
const char kDefaultSiteViewUsed[] = "default_site_view_used";
const char kOverflowMenuNoHorizontalScrollOrAction[] =
    "overflow_menu_no_horizontal_scroll_or_action";
const char kPriceNotificationsUsed[] = "price_notifications_used";
const char kDefaultBrowserPromoShown[] = "default_browser_promo_shown";
const char kDefaultBrowserPromoRemindMeLater[] =
    "default_browser_promo_remind_me_later";
const char kPasswordManagerWidgetPromoTriggered[] =
    "password_manager_widget_promo_trigger";
const char kPasswordManagerWidgetPromoUsed[] =
    "password_manager_widget_promo_used";
const char kPasswordManagerWidgetPromoClosed[] =
    "password_manager_widget_promo_closed";

// Default browser blue dot promo.
const char kBlueDotPromoOverflowMenuShown[] =
    "blue_dot_promo_overflow_menu_shown";
const char kBlueDotPromoSettingsShown[] = "blue_dot_promo_settings_shown";
const char kBlueDotPromoOverflowMenuOpened[] =
    "blue_dot_promo_overflow_menu_opened";
const char kBlueDotPromoSettingsDismissed[] =
    "blue_dot_promo_settings_dismissed";
const char kBlueDotOverflowMenuCustomized[] =
    "blue_dot_overflow_menu_customized";
const char kBlueDotPromoOverflowMenuDismissed[] =
    "blue_dot_promo_overflow_menu_dismissed";

const char kCredentialProviderExtensionPromoSnoozed[] =
    "credential_provider_extension_promo_snoozed";
const char kDockingPromoRemindMeLater[] = "docking_promo_remind_me_later";
const char kOpenUrlFromOmnibox[] = "open_url_from_omnibox";
const char kHistoryOnOverflowMenuUsed[] = "history_on_overflow_menu_used";
const char kLensButtonKeyboardUsed[] = "lens_keyboard_used";
const char kParcelTrackingTriggered[] = "parcel_tracking_triggered";
const char kParcelTracked[] = "parcel_tracked";
const char kIOSMultiGestureRefreshUsed[] = "multi_gesture_refresh_used";
const char kIOSPullToRefreshUsed[] = "pull_to_refresh_feature_used";
const char kIOSPullToRefreshIPHDismissButtonTapped[] =
    "pull_to_refresh_feature_iph_dismiss_button_tapped";
const char kIOSIncognitoPageControlTapped[] = "incognito_page_control_tapped";
const char kIOSSwipeRightForIncognitoUsed[] = "swipe_right_for_incognito_used";
const char kIOSSwipeRightForIncognitoIPHDismissButtonTapped[] =
    "swipe_right_for_incognito_iph_dismiss_button_tapped";
const char kIOSBackForwardButtonTapped[] = "back_forward_button_tapped";
const char kIOSSwipeBackForwardUsed[] = "swiped_back_forward_used";
const char kIOSSwipeBackForwardIPHDismissButtonTapped[] =
    "swipe_back_forward_iph_dismiss_button_tapped";
const char kIOSTabGridAdjacentTabTapped[] = "tab_grid_adjacent_tab_tapped";
const char kIOSSwipeToolbarToChangeTabUsed[] =
    "swipe_toolbar_to_change_tab_used";
const char kIOSSwipeToolbarToChangeTabIPHDismissButtonTapped[] =
    "swipe_toolbar_to_change_tab_iph_dismiss_button_tapped";
const char kIOSOverflowMenuCustomizationUsed[] =
    "overflow_menu_customization_used";
const char kIOSOverflowMenuOffscreenItemUsed[] =
    "overflow_menu_offscreen_item_used";
const char kIOSDefaultBrowserFREShown[] = "default_browser_fre_shown";
const char kGenericDefaultBrowserPromoConditionsMet[] =
    "generic_default_browser_promo_conditions_met";
const char kAllTabsPromoConditionsMet[] = "all_tabs_promo_conditions_met";
const char kMadeForIOSPromoConditionsMet[] =
    "made_for_ios_promo_conditions_met";
const char kStaySafePromoConditionsMet[] = "stay_safe_promo_conditions_met";
const char kEnhancedSafeBrowsingPromoCriterionMet[] =
    "enhanced_safe_browsing_promo_criterion_met";
const char kInlineEnhancedSafeBrowsingPromoClosed[] =
    "inline_enhanced_safe_browsing_promo_closed";
const char kGenericDefaultBrowserPromoTrigger[] =
    "generic_default_browser_promo_trigger";
const char kAllTabsPromoTrigger[] = "all_tabs_promo_trigger";
const char kMadeForIOSPromoTrigger[] = "made_for_ios_promo_trigger";
const char kStaySafePromoTrigger[] = "stay_safe_promo_trigger";
const char kTailoredDefaultBrowserPromosGroupTrigger[] =
    "tailored_default_browser_promos_group_trigger";
const char kDefaultBrowserPromoTriggerCriteriaConditionsMet[] =
    "default_browser_promo_trigger_criteria_conditions_met";
const char kIOSContextualPanelSampleModelEntrypointUsed[] =
    "ios_contextual_panel_sample_model_entrypoint_used";
const char kIOSContextualPanelPriceInsightsEntrypointUsed[] =
    "ios_contextual_panel_price_insights_entrypoint_used";
const char kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed[] =
    "ios_contextual_panel_price_insights_entrypoint_explicitly_dismissed";
const char kHomeCustomizationMenuUsed[] = "home_customization_menu_used";
const char kLensOverlayEntrypointUsed[] = "lens_overlay_entrypoint_used";
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
const char kPwaInstallMenuSelected[] = "pwa_install_menu_clicked";
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace events

}  // namespace feature_engagement
