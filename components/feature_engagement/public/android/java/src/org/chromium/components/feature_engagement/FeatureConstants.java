// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

import androidx.annotation.StringDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * FeatureConstants contains the String name of all base::Feature in-product help features declared
 * in //components/feature_engagement/public/feature_constants.h.
 */
@StringDef({FeatureConstants.DOWNLOAD_PAGE_FEATURE,
        FeatureConstants.DOWNLOAD_PAGE_SCREENSHOT_FEATURE, FeatureConstants.DOWNLOAD_HOME_FEATURE,
        FeatureConstants.CHROME_DUET_FEATURE, FeatureConstants.CHROME_HOME_EXPAND_FEATURE,
        FeatureConstants.CHROME_HOME_PULL_TO_REFRESH_FEATURE,
        FeatureConstants.DATA_SAVER_PREVIEW_FEATURE, FeatureConstants.DATA_SAVER_DETAIL_FEATURE,
        FeatureConstants.PREVIEWS_OMNIBOX_UI_FEATURE,
        FeatureConstants.TRANSLATE_MENU_BUTTON_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_WEB_SEARCH_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_OPT_IN_FEATURE,
        FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE,
        FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE,
        FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE,
        FeatureConstants.DOWNLOAD_SETTINGS_FEATURE,
        FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE,
        FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOADS_ARE_FASTER_FEATURE,
        FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE,
        FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE,
        FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE,
        FeatureConstants.FEED_CARD_MENU_FEATURE, FeatureConstants.IDENTITY_DISC_FEATURE,
        FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE})
@Retention(RetentionPolicy.SOURCE)
public @interface FeatureConstants {
    String DOWNLOAD_PAGE_FEATURE = "IPH_DownloadPage";
    String DOWNLOAD_PAGE_SCREENSHOT_FEATURE = "IPH_DownloadPageScreenshot";
    String DOWNLOAD_HOME_FEATURE = "IPH_DownloadHome";
    String CHROME_DUET_FEATURE = "IPH_ChromeDuet";
    String CHROME_HOME_EXPAND_FEATURE = "IPH_ChromeHomeExpand";
    String CHROME_HOME_PULL_TO_REFRESH_FEATURE = "IPH_ChromeHomePullToRefresh";
    String DATA_SAVER_PREVIEW_FEATURE = "IPH_DataSaverPreview";
    String DATA_SAVER_DETAIL_FEATURE = "IPH_DataSaverDetail";
    String DATA_SAVER_MILESTONE_PROMO_FEATURE = "IPH_DataSaverMilestonePromo";
    String KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE = "IPH_KeyboardAccessoryAddressFilling";
    String KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE = "IPH_KeyboardAccessoryPasswordFilling";
    String KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE = "IPH_KeyboardAccessoryPaymentFilling";
    String PREVIEWS_OMNIBOX_UI_FEATURE = "IPH_PreviewsOmniboxUI";
    String TRANSLATE_MENU_BUTTON_FEATURE = "IPH_TranslateMenuButton";
    String EXPLORE_SITES_TILE_FEATURE = "IPH_ExploreSitesTile";

    /**
     * An IPH feature that encourages users who search a query from a web page in a new tab, to use
     * Contextual Search instead.
     */
    String CONTEXTUAL_SEARCH_WEB_SEARCH_FEATURE = "IPH_ContextualSearchWebSearch";

    /**
     * An IPH feature for promoting tap over longpress for activating Contextual Search.
     */
    String CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE = "IPH_ContextualSearchPromoteTap";

    /**
     * An IPH feature for encouraging users to open the Contextual Search Panel.
     */
    String CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE = "IPH_ContextualSearchPromotePanelOpen";

    /**
     * An IPH feature for encouraging users to opt-in for Contextual Search.
     */
    String CONTEXTUAL_SEARCH_OPT_IN_FEATURE = "IPH_ContextualSearchOptIn";

    /**
     * An IPH feature indicating to users that there are settings for downloads and they are
     * accessible through Downloads Home.
     */
    String DOWNLOAD_SETTINGS_FEATURE = "IPH_DownloadSettings";

    /**
     * An IPH feature informing the users that even though infobar was closed, downloads are still
     * continuing in the background.
     */
    String DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE = "IPH_DownloadInfoBarDownloadContinuing";

    /**
     * An IPH feature that points to the download progress infobar and informs users that downloads
     * are now faster than before.
     */
    String DOWNLOAD_INFOBAR_DOWNLOADS_ARE_FASTER_FEATURE = "IPH_DownloadInfoBarDownloadsAreFaster";

    /**
     * An IPH feature to prompt the user to long press on pages with links to open them in a group.
     */
    String TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE = "IPH_TabGroupsQuicklyComparePages";

    /**
     * An IPH feature to show when the tabstrip shows to explain what each button does.
     */
    String TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE = "IPH_TabGroupsTapToSeeAnotherTab";

    /**
     * An IPH feature to show on tab switcher cards with multiple tab thumbnails.
     */
    String TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE = "IPH_TabGroupsYourTabsTogether";

    /**
     * An IPH feature to show a card item on grid tab switcher to educate drag-and-drop.
     */
    String TAB_GROUPS_DRAG_AND_DROP_FEATURE = "IPH_TabGroupsDragAndDrop";

    /**
     * An IPH feature to show on a card menu on the FeedNewTabPage.
     */
    String FEED_CARD_MENU_FEATURE = "IPH_FeedCardMenu";

    /**
     * An IPH feature prompting user to tap on identity disc to navigate to "Sync and Google
     * services" preferences.
     */
    String IDENTITY_DISC_FEATURE = "IPH_IdentityDisc";
}
