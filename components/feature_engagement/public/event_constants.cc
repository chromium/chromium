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

const char kSplitViewCreated[] = "split_view_created";

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
const char kViewedWhatsNew[] = "viewed_whats_new_m143";
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
const char kNonModalDefaultBrowserPromoUrlPasteTrigger[] =
    "non_modal_default_browser_promo_url_paste_trigger";
const char kNonModalDefaultBrowserPromoAppSwitcherTrigger[] =
    "non_modal_default_browser_promo_app_switcher_trigger";
const char kNonModalDefaultBrowserPromoShareTrigger[] =
    "non_modal_default_browser_promo_share_trigger";
const char kNonModalSigninPromoPasswordTrigger[] =
    "ios_non_modal_signin_promo_password_trigger";
const char kNonModalSigninPromoBookmarkTrigger[] =
    "ios_non_modal_signin_promo_bookmark_trigger";
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
const char kSettingsOnOverflowMenuUsed[] = "settings_on_overflow_menu_used";
const char kLensButtonKeyboardUsed[] = "lens_keyboard_used";
const char kIOSMultiGestureRefreshUsed[] = "multi_gesture_refresh_used";
const char kIOSPullToRefreshUsed[] = "pull_to_refresh_feature_used";
const char kIOSPullToRefreshIPHDismissButtonTapped[] =
    "pull_to_refresh_feature_iph_dismiss_button_tapped";
const char kIOSIncognitoPageControlTapped[] = "incognito_page_control_tapped";
const char kIOSSigninFullscreenPromoTrigger[] =
    "signin_fullscreen_promo_trigger";
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
const char kIOSContextualPanelSampleModelEntrypointUsed[] =
    "ios_contextual_panel_sample_model_entrypoint_used";
const char kIOSContextualPanelPriceInsightsEntrypointUsed[] =
    "ios_contextual_panel_price_insights_entrypoint_used";
const char kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed[] =
    "ios_contextual_panel_price_insights_entrypoint_explicitly_dismissed";
const char kHomeCustomizationMenuUsed[] = "home_customization_menu_used";
const char kLensOverlayEntrypointUsed[] = "lens_overlay_entrypoint_used";
const char kIOSLensButtonUsed[] = "ios_lens_button_used";
const char kIOSTabReminderScheduled[] = "tab_reminder_scheduled";
const char kIOSReminderNotificationsOverflowMenuBubbleIPHTrigger[] =
    "ios_reminder_notifications_overflow_menu_bubble_iph_trigger";
const char kIOSOverflowMenuSetTabReminderTapped[] =
    "ios_overflow_menu_set_tab_reminder_tapped";
const char kIOSReminderNotificationsOverflowMenuNewBadgeIPHTrigger[] =
    "ios_reminder_notifications_overflow_menu_new_badge_iph_trigger";
const char kIOSDownloadAutoDeletionIPHCriterionMet[] =
    "ios_download_auto_deletion_iph_criterion_met";
const char kIOSScrolledOnFeed[] = "ios_scrolled_on_feed";
const char kIOSActionOnFeed[] = "ios_action_on_feed";
const char kIOSWelcomeBackPromoTrigger[] = "welcome_back_promo_trigger";
const char kIOSWelcomeBackPromoUsed[] = "welcome_back_promo_used";
const char kIOSBWGPromoTrigger[] = "bwg_half_screen_promo_trigger";
const char kIOSBWGPromoUsed[] = "bwg_half_screen_promo_used";
const char kIOSSafariImportRemindMeLater[] =
    "ios_safari_import_entry_point_remind_me_later";
const char kIOSPageActionMenuIPHTrigger[] = "page_action_menu_iph_trigger";
const char kIOSPageActionMenuIPHUsed[] = "page_action_menu_iph_used";
const char kIOSFirstRunComplete[] = "ios_first_run_complete";
const char kIOSFREBadgeHoldbackPeriodElapsed[] =
    "ios_fre_badge_holdback_period_elapsed";
const char kIOSReaderModeUsed[] = "ios_reader_mode_used";
const char kIOSIPHBadgedReaderModeTriggered[] =
    "ios_iph_badged_reader_mode_triggered";
const char kIOSAIHubNewBadgeTriggered[] = "ios_new_ai_hub_badge_triggered";
const char kIOSAIHubNewBadgeUsed[] = "ios_new_ai_hub_badge_used";
const char kIOSFullscreenPromosGroupTrigger[] =
    "fullscreen_promos_group_trigger";
const char kIOSGeminiContextualCueChipTriggered[] =
    "ios_gemini_contextual_cue_chip_triggered";
const char kIOSGeminiContextualCueChipUsed[] =
    "ios_gemini_contextual_cue_chip_used";
const char kIOSGeminiPromoFirstCompletion[] =
    "ios_gemini_promo_first_completion";
const char kIOSGeminiEligiblity[] = "ios_gemini_eligiblity";
const char kIOSIPHReaderModeOptionsUsed[] = "ios_iph_reader_mode_options_used";
const char kIOSIPHReaderModeOptionsTriggered[] =
    "ios_iph_reader_mode_options_triggered";
const char kIOSGeminiFullscreenPromoTriggered[] =
    "ios_gemini_fullscreen_promo_triggered";
const char kIOSGeminiFlowStartedNonPromo[] =
    "ios_gemini_flow_started_non_promo";
const char kIOSGeminiConsentGiven[] = "ios_gemini_consent_given";
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
const char kPwaInstallMenuSelected[] = "pwa_install_menu_clicked";
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace events

}  // namespace feature_engagement
