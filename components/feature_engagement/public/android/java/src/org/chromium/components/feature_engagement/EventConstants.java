// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

/** EventConstants contains the String name of all in-product help events. */
public final class EventConstants {
    /** The page load has failed and user has landed on an offline dino page. */
    public static final String USER_HAS_SEEN_DINO = "user_has_seen_dino";

    /** The user has started downloading a page. */
    public static final String DOWNLOAD_PAGE_STARTED = "download_page_started";

    /** The download has completed successfully. */
    public static final String DOWNLOAD_COMPLETED = "download_completed";

    /** The download home was opened by the user (from toolbar menu or notifications). */
    public static final String DOWNLOAD_HOME_OPENED = "download_home_opened";

    /** Screenshot is taken with Chrome in the foreground. */
    public static final String SCREENSHOT_TAKEN_CHROME_IN_FOREGROUND =
            "screenshot_taken_chrome_in_foreground";

    /** Add to homescreen events. */
    public static final String ADD_TO_HOMESCREEN_DIALOG_SHOWN = "add_to_homescreen_dialog_shown";

    /** The user clicked the minimize button on the toolbar. */
    public static final String CCT_MINIMIZE_BUTTON_CLICKED = "cct_minimize_button_clicked";

    /** The user clicked the history item on the menu in the custom tab toolbar. */
    public static final String CCT_HISTORY_MENU_ITEM_CLICKED = "cct_history_menu_item_clicked";

    /** The user clicked the search button on the history toolbar. */
    public static final String HISTORY_TOOLBAR_SEARCH_MENU_ITEM_CLICKED =
            "history_toolbar_search_menu_item_clicked";

    /**
     * User performed a web search for a query by choosing the Web Search option on the popup menu.
     */
    public static final String WEB_SEARCH_PERFORMED = "web_search_performed";

    /** The partner homepage was pressed. */
    public static final String PARTNER_HOME_PAGE_BUTTON_PRESSED =
            "partner_home_page_button_pressed";

    /** The homepage button in the toolbar was clicked. */
    public static final String HOMEPAGE_BUTTON_CLICKED = "homepage_button_clicked";

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

    /** The keyboard accessory was used to create a new plus address. */
    public static final String KEYBOARD_ACCESSORY_PLUS_ADDRESS_CREATE_SUGGESTION =
            "keyboard_accessory_plus_address_create_suggestion";

    /** User has finished drop-to-merge to create a group. */
    public static final String TAB_DRAG_AND_DROP_TO_GROUP = "tab_drag_and_drop_to_group";

    /** User has tapped on Identity Disc. */
    public static final String IDENTITY_DISC_USED = "identity_disc_used";

    /** User has used Ephemeral Tab i.e. opened and browsed the content. */
    public static final String EPHEMERAL_TAB_USED = "ephemeral_tab_used";

    /** 'Manage windows' menu for multi-instance support feature was tapped. */
    public static final String INSTANCE_SWITCHER_IPH_USED = "instance_switcher_iph_used";

    public static final String TAB_SWITCHER_BUTTON_CLICKED = "tab_switcher_button_clicked";
    public static final String TAB_SWITCHER_BUTTON_LONG_CLICKED =
            "tab_switcher_button_long_clicked";

    public static final String FOREGROUND_SESSION_DESTROYED = "foreground_session_destroyed";

    /** Read later related events. */
    public static final String APP_MENU_BOOKMARK_STAR_ICON_PRESSED =
            "app_menu_bookmark_star_icon_pressed";

    public static final String READ_LATER_CONTEXT_MENU_TAPPED = "read_later_context_menu_tapped";
    public static final String READ_LATER_ARTICLE_SAVED = "read_later_article_saved";
    public static final String READ_LATER_BOOKMARK_FOLDER_OPENED =
            "read_later_bookmark_folder_opened";

    /** The request desktop site window setting IPH was shown. */
    public static final String REQUEST_DESKTOP_SITE_WINDOW_SETTING_IPH_SHOWN =
            "request_desktop_site_window_setting_iph_shown";

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

    /** Desktop site settings page opened. */
    public static final String DESKTOP_SITE_SETTINGS_PAGE_OPENED =
            "desktop_site_settings_page_opened";

    /** Desktop site default-on message primary action event. */
    public static final String DESKTOP_SITE_DEFAULT_ON_PRIMARY_ACTION =
            "desktop_site_default_on_primary_action";

    /** Desktop site default-on message gesture event. */
    public static final String DESKTOP_SITE_DEFAULT_ON_GESTURE = "desktop_site_default_on_gesture";

    /** An app menu desktop site exception addition event. */
    public static final String APP_MENU_DESKTOP_SITE_EXCEPTION_ADDED =
            "app_menu_desktop_site_exception_added";

    /** Restore tabs related events. */
    public static final String RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO =
            "restore_tabs_on_first_run_show_promo";

    public static final String RESTORE_TABS_PROMO_USED = "restore_tabs_promo_used";

    public static final String TAB_GROUP_SYNC_ON_STRIP_USED = "tab_group_sync_on_strip_used";

    /** Description text for tab group sync functionality in the tab group creation dialog. */
    public static final String TAB_GROUP_CREATION_DIALOG_SHOWN = "tab_group_creation_dialog_shown";

    /** IPH dialog of RTL gesture navigation dialog is shown. */
    public static final String RTL_GESTURE_NAVIGATION_DIALOG_SHOW = "rtl_gesture_iph_show";

    /** Do not instantiate. */
    private EventConstants() {}
}
