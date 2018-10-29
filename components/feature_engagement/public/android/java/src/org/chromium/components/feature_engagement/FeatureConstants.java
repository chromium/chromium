// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

/**
 * FeatureConstants contains the String name of all base::Feature in-product help features declared
 * in //components/feature_engagement/public/feature_constants.h.
 */
public final class FeatureConstants {
    public static final String DOWNLOAD_PAGE_FEATURE = "IPH_DownloadPage";
    public static final String DOWNLOAD_PAGE_SCREENSHOT_FEATURE = "IPH_DownloadPageScreenshot";
    public static final String DOWNLOAD_HOME_FEATURE = "IPH_DownloadHome";
    public static final String CHROME_DUET_FEATURE = "IPH_ChromeDuet";
    public static final String CHROME_HOME_EXPAND_FEATURE = "IPH_ChromeHomeExpand";
    public static final String CHROME_HOME_PULL_TO_REFRESH_FEATURE = "IPH_ChromeHomePullToRefresh";
    public static final String CONTEXTUAL_SUGGESTIONS_FEATURE = "IPH_ContextualSuggestions";
    public static final String DATA_SAVER_PREVIEW_FEATURE = "IPH_DataSaverPreview";
    public static final String DATA_SAVER_DETAIL_FEATURE = "IPH_DataSaverDetail";
    public static final String NTP_BUTTON_FEATURE = "IPH_NewTabPageButton";
    public static final String PREVIEWS_OMNIBOX_UI_FEATURE = "IPH_PreviewsOmniboxUI";
    public static final String HOMEPAGE_TILE_FEATURE = "IPH_HomepageTile";

    public static final String MEDIA_DOWNLOAD_FEATURE = "IPH_MediaDownload";

    /**
     * An IPH feature that encourages users who search a query from a web page in a new tab, to use
     * Contextual Search instead.
     */
    public static final String CONTEXTUAL_SEARCH_WEB_SEARCH_FEATURE =
            "IPH_ContextualSearchWebSearch";

    /**
     * An IPH feature for promoting tap over longpress for activating Contextual Search.
     */
    public static final String CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE =
            "IPH_ContextualSearchPromoteTap";

    /**
     * An IPH feature for encouraging users to open the Contextual Search Panel.
     */
    public static final String CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE =
            "IPH_ContextualSearchPromotePanelOpen";

    /**
     * An IPH feature for encouraging users to opt-in for Contextual Search.
     */
    public static final String CONTEXTUAL_SEARCH_OPT_IN_FEATURE = "IPH_ContextualSearchOptIn";

    /**
     * An IPH feature indicating to users that there are settings for downloads and they are
     * accessible through Downloads Home.
     */
    public static final String DOWNLOAD_SETTINGS_FEATURE = "IPH_DownloadSettings";

    /**
     * An IPH feature informing the users that even though infobar was closed, downloads are still
     * continuing in the background.
     */
    public static final String DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE =
            "IPH_DownloadInfoBarDownloadContinuing";

    /**
     * An IPH feature that points to the download progress infobar and informs users that downloads
     * are now faster than before.
     */
    public static final String DOWNLOAD_INFOBAR_DOWNLOADS_ARE_FASTER_FEATURE =
            "IPH_DownloadInfoBarDownloadsAreFaster";

    /**
     * Do not instantiate.
     */
    private FeatureConstants() {}
}
