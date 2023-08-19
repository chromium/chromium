// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

/**
 * EventConstants contains the String name of all in-product help events.
 */
public final class EventConstants {
    /**
     * The page load has failed and user has landed on an offline dino page.
     */
    public static final String USER_HAS_SEEN_DINO = "user_has_seen_dino";

    /**
     * The user has started downloading a page.
     */
    public static final String DOWNLOAD_PAGE_STARTED = "download_page_started";

    /**
     * The download has completed successfully.
     */
    public static final String DOWNLOAD_COMPLETED = "download_completed";

    /**
     * The download home was opened by the user (from toolbar menu or notifications).
     */
    public static final String DOWNLOAD_HOME_OPENED = "download_home_opened";

    /**
     * Screenshot is taken with Chrome in the foreground.
     */
    public static final String SCREENSHOT_TAKEN_CHROME_IN_FOREGROUND =
            "screenshot_taken_chrome_in_foreground";

    /**
     * The data saver preview infobar was shown.
     */
    public static final String DATA_SAVER_PREVIEW_INFOBAR_SHOWN = "data_saver_preview_opened";

    /**
     * Data was saved when page loaded.
     */
    public static final String DATA_SAVED_ON_PAGE_LOAD = "data_saved_page_load";

    /**
     * The overflow menu was opened.
     */
    public static final String OVERFLOW_OPENED_WITH_DATA_SAVER_SHOWN =
            "overflow_opened_data_saver_shown";

    /**
     * The data saver footer was used (tapped).
     */
    public static final String DATA_SAVER_DETAIL_OPENED = "data_saver_overview_opened";

    /**
     * The data saver milestone promo was used (tapped).
     */
    public static final String DATA_SAVER_MILESTONE_PROMO_OPENED = "data_saver_milestone_promo";

    /**
     * The previews verbose status view was opened.
     */
    public static final String PREVIEWS_VERBOSE_STATUS_OPENED = "previews_verbose_status_opened";

    /**
     * A page load used a preview.
     */
    public static final String PREVIEWS_PAGE_LOADED = "preview_page_load";

    /**
     * Add to homescreen events.
     */
    public static final String ADD_TO_HOMESCREEN_DIALOG_SHOWN = "add_to_homescreen_dialog_shown";

    /**
     * Contextual Search panel was opened.
     */
    public static final String CONTEXTUAL_SEARCH_PANEL_OPENED = "contextual_search_panel_opened";

    /**
     * Contextual Search panel was opened after it was triggered by tapping.
     */
    public static final String CONTEXTUAL_SEARCH_PANEL_OPENED_AFTER_TAP =
            "contextual_search_panel_opened_after_tap";

    /**
     * Contextual Search panel was opened after it was triggered by longpressing.
     */
    public static final String CONTEXTUAL_SEARCH_PANEL_OPENED_AFTER_LONGPRESS =
            "contextual_search_panel_opened_after_longpress";

    /**
     * Contextual Search panel was opened after receiving entity data.
     */
    public static final String CONTEXTUAL_SEARCH_PANEL_OPENED_FOR_ENTITY =
            "contextual_search_panel_opened_for_entity";

    /**
     * User performed a web search for a query by choosing the Web Search option on the popup menu.
     */
    public static final String WEB_SEARCH_PERFORMED = "web_search_performed";

    /**
     * Contextual Search showed an entity result for the searched query.
     */
    public static final String CONTEXTUAL_SEARCH_ENTITY_RESULT = "contextual_search_entity_result";

    /**
     * Contextual Search was triggered by a tap.
     */
    public static final String CONTEXTUAL_SEARCH_TRIGGERED_BY_TAP =
            "contextual_search_triggered_by_tap";

    /**
     * Contextual Search was triggered by longpressing.
     */
    public static final String CONTEXTUAL_SEARCH_TRIGGERED_BY_LONGPRESS =
            "contextual_search_triggered_by_longpress";

    /**
     * Contextual Search attempted-trigger by Tap when user should Long-press.
     */
    public static final String CONTEXTUAL_SEARCH_TAPPED_BUT_SHOULD_LONGPRESS =
            "contextual_search_tapped_but_should_longpress";

    /**
     * Contextual Search acknowledged the suggestion that they should longpress instead of tap.
     */
    public static final String CONTEXTUAL_SEARCH_ACKNOWLEDGED_IN_PANEL_HELP =
            "contextual_search_acknowledged_in_panel_help";

    /**
     * Contextual Search user fully enabled access to page content through the opt-in.
     */
    public static final String CONTEXTUAL_SEARCH_ENABLED_OPT_IN =
            "contextual_search_enabled_opt_in";

    /**
     * The partner homepage was pressed.
     */
    public static final String PARTNER_HOME_PAGE_BUTTON_PRESSED =
            "partner_home_page_button_pressed";

    /** The homepage button in the toolbar was clicked. */
    public static final String HOMEPAGE_BUTTON_CLICKED = "homepage_button_clicked";

    /** The clear tab button in the toolbar was clicked. */
    public static final String CLEAR_TAB_BUTTON_CLICKED = "clear_tab_button_clicked";

    /** The pinned homepage tile in MV tiles was clicked. */
    public static final String HOMEPAGE_TILE_CLICKED = "homepage_tile_clicked";

    /** The `Translate` app menu button was clicked. */
    public static final String TRANSLATE_MENU_BUTTON_CLICKED = "translate_menu_button_clicked";

    /** The keyboard accessory was used to fill address data into a form. */
    public static final String KEYBOARD_ACCESSORY_ADDRESS_AUTOFILLED =
            "keyboard_accessory_address_suggestion_accepted";

    /** The keyboard accessory was used to fill a password form. */
    public static final String KEYBOARD_ACCESSORY_PASSWORD_AUTOFILLED =
            "keyboard_accessory_password_suggestion_accepted";

    /** The keyboard accessory was used to fill payment data into a form. */
    public static final String KEYBOARD_ACCESSORY_PAYMENT_AUTOFILLED =
            "autofill_virtual_card_suggestion_accepted";

    /** The keyboard accessory was swiped to reveal more suggestions. */
    public static final String KEYBOARD_ACCESSORY_BAR_SWIPED = "keyboard_accessory_bar_swiped";

    /** The keyboard accessory was used to fill virtual card CVC data into a form. */
    public static final String KEYBOARD_ACCESSORY_VIRTUAL_CARD_CVC_AUTOFILLED =
            "autofill_virtual_card_cvc_suggestion_accepted";

    /** User has finished drop-to-merge to create a group. */
    public static final String TAB_DRAG_AND_DROP_TO_GROUP = "tab_drag_and_drop_to_group";

    /** User has tapped on Identity Disc. */
    public static final String IDENTITY_DISC_USED = "identity_disc_used";

    /** User has used Ephemeral Tab i.e. opened and browsed the content. */
    public static final String EPHEMERAL_TAB_USED = "ephemeral_tab_used";

    /** 'Manage windows' menu for multi-instance support feature was tapped. */
    public static final String INSTANCE_SWITCHER_IPH_USED = "instance_switcher_iph_used";

    public static final String TAB_SWITCHER_BUTTON_CLICKED = "tab_switcher_button_clicked";

    public static final String FOREGROUND_SESSION_DESTROYED = "foreground_session_destroyed";

    /** Read later related events. */
    public static final String APP_MENU_BOOKMARK_STAR_ICON_PRESSED =
            "app_menu_bookmark_star_icon_pressed";
    public static final String READ_LATER_CONTEXT_MENU_TAPPED = "read_later_context_menu_tapped";
    public static final String READ_LATER_ARTICLE_SAVED = "read_later_article_saved";
    public static final String READ_LATER_BOTTOM_SHEET_FOLDER_SEEN =
            "read_later_bottom_sheet_folder_seen";
    public static final String READ_LATER_BOOKMARK_FOLDER_OPENED =
            "read_later_bookmark_folder_opened";

    /** Video tutorial related events. */
    public static final String VIDEO_TUTORIAL_DISMISSED_SUMMARY =
            "video_tutorial_iph_dismissed_summary";
    public static final String VIDEO_TUTORIAL_DISMISSED_CHROME_INTRO =
            "video_tutorial_iph_dismissed_chrome_intro";
    public static final String VIDEO_TUTORIAL_DISMISSED_DOWNLOAD =
            "video_tutorial_iph_dismissed_download";
    public static final String VIDEO_TUTORIAL_DISMISSED_SEARCH =
            "video_tutorial_iph_dismissed_search";
    public static final String VIDEO_TUTORIAL_DISMISSED_VOICE_SEARCH =
            "video_tutorial_iph_dismissed_voice_search";
    public static final String VIDEO_TUTORIAL_CLICKED_SUMMARY =
            "video_tutorial_iph_clicked_summary";
    public static final String VIDEO_TUTORIAL_CLICKED_CHROME_INTRO =
            "video_tutorial_iph_clicked_chrome_intro";
    public static final String VIDEO_TUTORIAL_CLICKED_DOWNLOAD =
            "video_tutorial_iph_clicked_download";
    public static final String VIDEO_TUTORIAL_CLICKED_SEARCH = "video_tutorial_iph_clicked_search";
    public static final String VIDEO_TUTORIAL_CLICKED_VOICE_SEARCH =
            "video_tutorial_iph_clicked_voice_search";

    /** Reengagement events. */
    public static final String STARTED_FROM_MAIN_INTENT = "started_from_main_intent";

    /** PWA install events. */
    public static final String PWA_INSTALL_MENU_SELECTED = "pwa_install_menu_clicked";

    /** PageInfo events. */
    public static final String PAGE_INFO_OPENED = "page_info_opened";

    /** PageInfoStoreInfo events. */
    public static final String PAGE_INFO_STORE_INFO_ROW_CLICKED =
            "page_info_store_info_row_clicked";

    /** PageZoom events. */
    public static final String PAGE_ZOOM_OPENED = "page_zoom_opened";

    /** Permission events. */
    public static final String PERMISSION_REQUEST_SHOWN = "permission_request_shown";

    /** Screenshot events */
    public static final String SHARE_SCREENSHOT_SELECTED = "share_screenshot_clicked";

    /** Sharing Hub link toggle events. */
    public static final String SHARING_HUB_LINK_TOGGLE_CLICKED = "sharing_hub_link_toggle_clicked";

    /** Crow events. */
    public static final String CROW_TAB_MENU_ITEM_CLICKED = "crow_tab_menu_item_clicked";

    /** Mic toolbar IPH event */
    public static final String SUCCESSFUL_VOICE_SEARCH = "successful_voice_search";

    /** AdaptiveButtonInTopToolbarCustomization new tab events. */
    public static final String ADAPTIVE_TOOLBAR_CUSTOMIZATION_NEW_TAB_OPENED =
            "adaptive_toolbar_customization_new_tab_opened";

    /** AdaptiveButtonInTopToolbarCustomization share events. */
    public static final String ADAPTIVE_TOOLBAR_CUSTOMIZATION_SHARE_OPENED =
            "adaptive_toolbar_customization_share_opened";

    /** AdaptiveButtonInTopToolbarCustomization voice search events. */
    public static final String ADAPTIVE_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_OPENED =
            "adaptive_toolbar_customization_voice_search_opened";

    /** AdaptiveButtonInTopToolbarCustomization translate events. */
    public static final String ADAPTIVE_TOOLBAR_CUSTOMIZATION_TRANSLATE_OPENED =
            "adaptive_toolbar_customization_translate_opened";

    /** AdaptiveButtonInTopToolbarCustomization read aloud events. */
    public static final String ADAPTIVE_TOOLBAR_CUSTOMIZATION_READ_ALOUD_CLICKED =
            "adaptive_toolbar_customization_read_aloud_clicked";

    /** AdaptiveButtonInTopToolbarCustomization add to bookmarks events. */
    public static final String ADAPTIVE_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_OPENED =
            "adaptive_toolbar_customization_add_to_bookmarks_opened";

    /** Open new incognito tab from app menu. */
    public static final String APP_MENU_NEW_INCOGNITO_TAB_CLICKED =
            "app_menu_new_incognito_tab_clicked";

    /** Voice search button click on NTP. */
    public static final String NTP_VOICE_SEARCH_BUTTON_CLICKED = "ntp_voice_search_button_clicked";

    /** WebFeed events. */
    public static final String WEB_FEED_FOLLOW_INTRO_CLICKED = "web_feed_follow_intro_clicked";

    /** Shared Highlighting button event */
    public static final String IPH_SHARED_HIGHLIGHTING_USED = "iph_shared_highlighting_used";

    /** Webnotes Stylize feature used from Sharing Hub */
    public static final String SHARING_HUB_WEBNOTES_STYLIZE_USED =
            "sharing_hub_webnotes_stylize_used";

    /** AutoDark disabled from app menu events. */
    public static final String AUTO_DARK_DISABLED_IN_APP_MENU = "auto_dark_disabled_in_app_menu";

    /** AutoDark theme settings opened while feature enabled. */
    public static final String AUTO_DARK_SETTINGS_OPENED = "auto_dark_settings_opened";

    /** The feed swipe refresh event. */
    public static final String FEED_SWIPE_REFRESHED = "feed_swipe_refresh_shown";

    /** The option to track the price of a product was selected from the main menu. */
    public static final String SHOPPING_LIST_PRICE_TRACK_FROM_MENU =
            "shopping_list_track_price_from_menu";

    /** A tap on the folder icon in the enhanced bookmark save flow. */
    public static final String SHOPPING_LIST_SAVE_FLOW_FOLDER_TAP =
            "shopping_list_save_flow_folder_tap";

    /** An app menu (tab-level) desktop site setting update event. */
    public static final String APP_MENU_DESKTOP_SITE_FOR_TAB_CLICKED =
            "app_menu_desktop_site_for_tab_clicked";

    /** Desktop site settings page opened. */
    public static final String DESKTOP_SITE_SETTINGS_PAGE_OPENED =
            "desktop_site_settings_page_opened";

    /** Desktop site default-on message primary action event. */
    public static final String DESKTOP_SITE_DEFAULT_ON_PRIMARY_ACTION =
            "desktop_site_default_on_primary_action";

    /** Desktop site default-on message gesture event. */
    public static final String DESKTOP_SITE_DEFAULT_ON_GESTURE = "desktop_site_default_on_gesture";

    /** Desktop site opt-in message primary action event. */
    public static final String DESKTOP_SITE_OPT_IN_PRIMARY_ACTION =
            "desktop_site_opt_in_primary_action";

    /** Desktop site opt-in message gesture event. */
    public static final String DESKTOP_SITE_OPT_IN_GESTURE = "desktop_site_opt_in_gesture";

    /** An app menu desktop site exception addition event. */
    public static final String APP_MENU_DESKTOP_SITE_EXCEPTION_ADDED =
            "app_menu_desktop_site_exception_added";

    /** Restore tabs related events. */
    public static final String RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO =
            "restore_tabs_on_first_run_show_promo";
    public static final String RESTORE_TABS_PROMO_USED = "restore_tabs_promo_used";

    /**
     * Do not instantiate.
     */
    private EventConstants() {}
}
