// Copyright 2017 The Chromium Authors. All rights reserved.
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
     * The user triggered pull to refresh. Used to help determine when to show the Chrome Home
     * in-product help.
     */
    public static final String PULL_TO_REFRESH = "pull_to_refresh";

    /** The contextual suggestions button was shown to the user. */
    public static final String CONTEXTUAL_SUGGESTIONS_BUTTON_SHOWN =
            "contextual_suggestions_button_shown";

    /**
     * The contextual suggestions bottom sheet was explicitly dismissed via a tap on its close
     * button.
     */
    public static final String CONTEXTUAL_SUGGESTIONS_DISMISSED =
            "contextual_suggestions_dismissed";

    /**
     * The contextual suggestions bottom sheet was opened.
     */
    public static final String CONTEXTUAL_SUGGESTIONS_OPENED = "contextual_suggestions_opened";

    /**
     * The contextual suggestions bottom sheet was shown in its peeking state.
     */
    public static final String CONTEXTUAL_SUGGESTIONS_PEEKED = "contextual_suggestions_peeked";

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
     * The previews verbose status view was opened.
     */
    public static final String PREVIEWS_VERBOSE_STATUS_OPENED = "previews_verbose_status_opened";

    /**
     * A page load used a preview.
     */
    public static final String PREVIEWS_PAGE_LOADED = "preview_page_load";

    /**
     * The download button for a media element was displayed.
     */
    public static final String MEDIA_DOWNLOAD_BUTTON_DISPLAYED = "media_download_button_displayed";

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
     * The partner homepage was pressed.
     */
    public static final String PARTNER_HOME_PAGE_BUTTON_PRESSED =
            "partner_home_page_button_pressed";

    /** The user used a button in the bottom toolbar. */
    public static final String CHROME_DUET_USED_BOTTOM_TOOLBAR = "chrome_duet_used_bottom_toolbar";

    /** The homepage button in the toolbar was clicked. */
    public static final String HOMEPAGE_BUTTON_CLICKED = "homepage_button_clicked";

    /** The clear tab button in the toolbar was clicked. */
    public static final String CLEAR_TAB_BUTTON_CLICKED = "clear_tab_button_clicked";

    /** The pinned homepage tile in MV tiles was clicked. */
    public static final String HOMEPAGE_TILE_CLICKED = "homepage_tile_clicked";

    /**
     * Do not instantiate.
     */
    private EventConstants() {}
}
