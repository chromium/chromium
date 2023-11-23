// Copyright 2017 The Chromium Authors
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
@StringDef({
    FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_NEW_TAB_FEATURE,
    FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_SHARE_FEATURE,
    FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_FEATURE,
    FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_TRANSLATE_FEATURE,
    FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_FEATURE,
    FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_READ_ALOUD_FEATURE,
    FeatureConstants.ADD_TO_HOMESCREEN_MESSAGE_FEATURE,
    FeatureConstants.AUTO_DARK_OPT_OUT_FEATURE,
    FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE,
    FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_OPT_IN_FEATURE,
    FeatureConstants.CCT_MINIMIZED_FEATURE,
    FeatureConstants.DOWNLOAD_PAGE_FEATURE,
    FeatureConstants.DOWNLOAD_PAGE_SCREENSHOT_FEATURE,
    FeatureConstants.DOWNLOAD_HOME_FEATURE,
    FeatureConstants.DOWNLOAD_INDICATOR_FEATURE,
    FeatureConstants.CHROME_HOME_EXPAND_FEATURE,
    FeatureConstants.CHROME_HOME_PULL_TO_REFRESH_FEATURE,
    FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
    FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP,
    FeatureConstants.COOKIE_CONTROLS_3PCD_FEATURE,
    FeatureConstants.COOKIE_CONTROLS_FEATURE,
    FeatureConstants.DATA_SAVER_PREVIEW_FEATURE,
    FeatureConstants.DATA_SAVER_DETAIL_FEATURE,
    FeatureConstants.DATA_SAVER_MILESTONE_PROMO_FEATURE,
    FeatureConstants.EPHEMERAL_TAB_FEATURE,
    FeatureConstants.PREVIEWS_OMNIBOX_UI_FEATURE,
    FeatureConstants.TRANSLATE_MENU_BUTTON_FEATURE,
    FeatureConstants.INSTANCE_SWITCHER,
    FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE,
    FeatureConstants.KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE,
    FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE,
    FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE,
    FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE,
    FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE,
    FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE,
    FeatureConstants.KEYBOARD_ACCESSORY_VIRTUAL_CARD_CVC_FILL_FEATURE,
    FeatureConstants.DOWNLOAD_SETTINGS_FEATURE,
    FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE,
    FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOADS_ARE_FASTER_FEATURE,
    FeatureConstants.FEATURE_NOTIFICATION_GUIDE_DEFAULT_BROWSER_PROMO_FEATURE,
    FeatureConstants.FEATURE_NOTIFICATION_GUIDE_INCOGNITO_TAB_HELP_BUBBLE_FEATURE,
    FeatureConstants.FEATURE_NOTIFICATION_GUIDE_NTP_SUGGESTION_CARD_HELP_BUBBLE_FEATURE,
    FeatureConstants.FEATURE_NOTIFICATION_GUIDE_SIGN_IN_HELP_BUBBLE_FEATURE,
    FeatureConstants.FEATURE_NOTIFICATION_GUIDE_VOICE_SEARCH_HELP_BUBBLE_FEATURE,
    FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE,
    FeatureConstants.SHOPPING_LIST_MENU_ITEM_FEATURE,
    FeatureConstants.SHOPPING_LIST_SAVE_FLOW_FEATURE,
    FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE,
    FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE,
    FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE,
    FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE,
    FeatureConstants.FEED_CARD_MENU_FEATURE,
    FeatureConstants.IDENTITY_DISC_FEATURE,
    FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE,
    FeatureConstants.QUIET_NOTIFICATION_PROMPTS_FEATURE,
    FeatureConstants.FEED_HEADER_MENU_FEATURE,
    FeatureConstants.FEED_SWIPE_REFRESH_FEATURE,
    FeatureConstants.WEB_FEED_AWARENESS_FEATURE,
    FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE,
    FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE,
    FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE,
    FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE,
    FeatureConstants.PAGE_INFO_FEATURE,
    FeatureConstants.PAGE_INFO_STORE_INFO_FEATURE,
    FeatureConstants.PAGE_ZOOM_FEATURE,
    FeatureConstants.READ_LATER_APP_MENU_BOOKMARK_THIS_PAGE_FEATURE,
    FeatureConstants.READ_LATER_APP_MENU_BOOKMARKS_FEATURE,
    FeatureConstants.READ_LATER_BOTTOM_SHEET_FEATURE,
    FeatureConstants.READ_LATER_CONTEXT_MENU_FEATURE,
    FeatureConstants.REQUEST_DESKTOP_SITE_APP_MENU_FEATURE,
    FeatureConstants.REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE,
    FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE,
    FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE,
    FeatureConstants.REQUEST_DESKTOP_SITE_WINDOW_SETTING_FEATURE,
    FeatureConstants.IPH_MIC_TOOLBAR_FEATURE,
    FeatureConstants.IPH_SHARE_SCREENSHOT_FEATURE,
    FeatureConstants.IPH_SHARING_HUB_LINK_TOGGLE_FEATURE,
    FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE,
    FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE,
    FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE_WITH_UI_UPDATE,
    FeatureConstants.SHARED_HIGHLIGHTING_BUILDER_FEATURE,
    FeatureConstants.SHARED_HIGHLIGHTING_RECEIVER_FEATURE,
    FeatureConstants.SHARING_HUB_WEBNOTES_STYLIZE_FEATURE,
    FeatureConstants.VIDEO_TUTORIAL_NTP_CHROME_INTRO_FEATURE,
    FeatureConstants.VIDEO_TUTORIAL_NTP_DOWNLOAD_FEATURE,
    FeatureConstants.VIDEO_TUTORIAL_NTP_SEARCH_FEATURE,
    FeatureConstants.VIDEO_TUTORIAL_NTP_SUMMARY_FEATURE,
    FeatureConstants.VIDEO_TUTORIAL_NTP_VOICE_SEARCH_FEATURE,
    FeatureConstants.VIDEO_TUTORIAL_TRY_NOW_FEATURE,
    FeatureConstants.PRICE_DROP_NTP_FEATURE,
    FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE
})
@Retention(RetentionPolicy.SOURCE)
public @interface FeatureConstants {
    String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_NEW_TAB_FEATURE =
            "IPH_AdaptiveButtonInTopToolbarCustomization_NewTab";
    String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_SHARE_FEATURE =
            "IPH_AdaptiveButtonInTopToolbarCustomization_Share";
    String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_FEATURE =
            "IPH_AdaptiveButtonInTopToolbarCustomization_VoiceSearch";
    String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_TRANSLATE_FEATURE =
            "IPH_AdaptiveButtonInTopToolbarCustomization_Translate";
    String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_FEATURE =
            "IPH_AdaptiveButtonInTopToolbarCustomization_AddToBookmarks";
    String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_READ_ALOUD_FEATURE =
            "IPH_AdaptiveButtonInTopToolbarCustomization_ReadAloud";
    String ADD_TO_HOMESCREEN_MESSAGE_FEATURE = "IPH_AddToHomescreenMessage";
    String AUTO_DARK_OPT_OUT_FEATURE = "IPH_AutoDarkOptOut";
    String AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE = "IPH_AutoDarkUserEducationMessage";
    String AUTO_DARK_USER_EDUCATION_MESSAGE_OPT_IN_FEATURE =
            "IPH_AutoDarkUserEducationMessageOptIn";
    String CCT_MINIMIZED_FEATURE = "IPH_CCTMinimized";
    String CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT = "IPH_ContextualPageActions_QuietVariant";
    String CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP = "IPH_ContextualPageActions_ActionChip";
    String COOKIE_CONTROLS_3PCD_FEATURE = "IPH_3pcdUserBypass";
    String COOKIE_CONTROLS_FEATURE = "IPH_CookieControls";
    String DOWNLOAD_PAGE_FEATURE = "IPH_DownloadPage";
    String DOWNLOAD_PAGE_SCREENSHOT_FEATURE = "IPH_DownloadPageScreenshot";
    String DOWNLOAD_HOME_FEATURE = "IPH_DownloadHome";
    String DOWNLOAD_INDICATOR_FEATURE = "IPH_DownloadIndicator";
    String CHROME_HOME_EXPAND_FEATURE = "IPH_ChromeHomeExpand";
    String CHROME_HOME_PULL_TO_REFRESH_FEATURE = "IPH_ChromeHomePullToRefresh";
    String DATA_SAVER_PREVIEW_FEATURE = "IPH_DataSaverPreview";
    String DATA_SAVER_DETAIL_FEATURE = "IPH_DataSaverDetail";
    String DATA_SAVER_MILESTONE_PROMO_FEATURE = "IPH_DataSaverMilestonePromo";
    String EPHEMERAL_TAB_FEATURE = "IPH_EphemeralTab";
    String KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE = "IPH_KeyboardAccessoryAddressFilling";
    String KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE = "IPH_KeyboardAccessoryPasswordFilling";
    String KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE = "IPH_KeyboardAccessoryPaymentFilling";
    String KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE = "IPH_KeyboardAccessoryPaymentOffer";
    String KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE = "IPH_AutofillVirtualCardSuggestion";
    String KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE =
            "IPH_AutofillExternalAccountProfileSuggestion";
    String KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE = "IPH_KeyboardAccessoryBarSwiping";
    String KEYBOARD_ACCESSORY_VIRTUAL_CARD_CVC_FILL_FEATURE =
            "IPH_AutofillVirtualCardCVCSuggestion";
    String INSTANCE_SWITCHER = "IPH_InstanceSwitcher";
    String PAGE_ZOOM_FEATURE = "IPH_PageZoom";
    String PREVIEWS_OMNIBOX_UI_FEATURE = "IPH_PreviewsOmniboxUI";
    String TRANSLATE_MENU_BUTTON_FEATURE = "IPH_TranslateMenuButton";
    String READ_LATER_CONTEXT_MENU_FEATURE = "IPH_ReadLaterContextMenu";
    String READ_LATER_APP_MENU_BOOKMARK_THIS_PAGE_FEATURE = "IPH_ReadLaterAppMenuBookmarkThisPage";
    String READ_LATER_APP_MENU_BOOKMARKS_FEATURE = "IPH_ReadLaterAppMenuBookmarks";
    String READ_LATER_BOTTOM_SHEET_FEATURE = "IPH_ReadLaterBottomSheet";
    String REQUEST_DESKTOP_SITE_APP_MENU_FEATURE = "IPH_RequestDesktopSiteAppMenu";
    String REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE = "IPH_RequestDesktopSiteDefaultOn";
    String REQUEST_DESKTOP_SITE_OPT_IN_FEATURE = "IPH_RequestDesktopSiteOptIn";
    String REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE =
            "IPH_RequestDesktopSiteExceptionsGeneric";
    String REQUEST_DESKTOP_SITE_WINDOW_SETTING_FEATURE = "IPH_RequestDesktopSiteWindowSetting";

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
     * An IPH feature attached to the mic button in the toolbar prompring user
     * to try voice.
     */
    String IPH_MIC_TOOLBAR_FEATURE = "IPH_MicToolbar";

    /** An IPH feature to prompt users to open the new tab page after a navigation. */
    String NEW_TAB_PAGE_HOME_BUTTON_FEATURE = "IPH_NewTabPageHomeButton";

    /** An IPH that shows in the bookmark save flow when bookmarking a product. */
    String SHOPPING_LIST_SAVE_FLOW_FEATURE = "IPH_ShoppingListSaveFlow";

    /**
     * An IPH that shows when a page is detected to be shopping related that shows the user a menu
     * item is available to track price.
     */
    String SHOPPING_LIST_MENU_ITEM_FEATURE = "IPH_ShoppingListMenuItem";

    /** An IPH feature to prompt the user to long press on pages with links to open them in a group. */
    String TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE = "IPH_TabGroupsQuicklyComparePages";

    /** An IPH feature to show when the tabstrip shows to explain what each button does. */
    String TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE = "IPH_TabGroupsTapToSeeAnotherTab";

    /** An IPH feature to show on tab switcher cards with multiple tab thumbnails. */
    String TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE = "IPH_TabGroupsYourTabsTogether";

    /** An IPH feature to prompt users to open the tab switcher after a navigation. */
    String TAB_SWITCHER_BUTTON_FEATURE = "IPH_TabSwitcherButton";

    /** An IPH feature to show a card item on grid tab switcher to educate drag-and-drop. */
    String TAB_GROUPS_DRAG_AND_DROP_FEATURE = "IPH_TabGroupsDragAndDrop";

    /**
     * An IPH feature to show a video tutorial card on NTP to educate about an introduction to
     * chrome.
     */
    String VIDEO_TUTORIAL_NTP_CHROME_INTRO_FEATURE = "IPH_VideoTutorial_NTP_ChromeIntro";

    /** An IPH feature to show a video tutorial card on NTP to educate about downloading in chrome. */
    String VIDEO_TUTORIAL_NTP_DOWNLOAD_FEATURE = "IPH_VideoTutorial_NTP_Download";

    /** An IPH feature to show a video tutorial card on NTP to educate about how to search in chrome. */
    String VIDEO_TUTORIAL_NTP_SEARCH_FEATURE = "IPH_VideoTutorial_NTP_Search";

    /**
     * An IPH feature to show a video tutorial card on NTP to educate about how to use voice search
     * in chrome.
     */
    String VIDEO_TUTORIAL_NTP_VOICE_SEARCH_FEATURE = "IPH_VideoTutorial_NTP_VoiceSearch";

    /**
     * An IPH feature to show a video tutorial summary card on NTP that takes them to see the video
     * tutorial list page.
     */
    String VIDEO_TUTORIAL_NTP_SUMMARY_FEATURE = "IPH_VideoTutorial_NTP_Summary";

    /**
     * An IPH feature to show an appropriate help bubble when user clicks on Try Now button on video
     * tutorial player.
     */
    String VIDEO_TUTORIAL_TRY_NOW_FEATURE = "IPH_VideoTutorial_TryNow";

    /** Feature notification guide help UI for default browser promo. */
    String FEATURE_NOTIFICATION_GUIDE_DEFAULT_BROWSER_PROMO_FEATURE =
            "IPH_FeatureNotificationGuideDefaultBrowserPromo";

    /** Feature notification guide help UI for sign in promo. */
    String FEATURE_NOTIFICATION_GUIDE_SIGN_IN_HELP_BUBBLE_FEATURE =
            "IPH_FeatureNotificationGuideSignInHelpBubble";

    /** Feature notification guide help UI for incognito tab. */
    String FEATURE_NOTIFICATION_GUIDE_INCOGNITO_TAB_HELP_BUBBLE_FEATURE =
            "IPH_FeatureNotificationGuideIncognitoTabHelpBubble";

    /** Feature notification guide help UI for NTP suggestion card. */
    String FEATURE_NOTIFICATION_GUIDE_NTP_SUGGESTION_CARD_HELP_BUBBLE_FEATURE =
            "IPH_FeatureNotificationGuideNTPSuggestionCardHelpBubble";

    /** Feature notification guide help UI for voice search. */
    String FEATURE_NOTIFICATION_GUIDE_VOICE_SEARCH_HELP_BUBBLE_FEATURE =
            "IPH_FeatureNotificationGuideVoiceSearchHelpBubble";

    /** An IPH feature to show on a card menu on the FeedNewTabPage. */
    String FEED_CARD_MENU_FEATURE = "IPH_FeedCardMenu";

    /** An IPH feature to prompt users to pull-to-refresh feed. */
    String FEED_SWIPE_REFRESH_FEATURE = "IPH_FeedSwipeRefresh";

    /** A generic IPH feature to always trigger help UI when asked. */
    String GENERIC_ALWAYS_TRIGGER_HELP_UI_FEATURE = "IPH_GenericAlwaysTriggerHelpUiFeature";

    /**
     * An IPH feature prompting user to tap on identity disc to navigate to "Sync and Google
     * services" preferences.
     */
    String IDENTITY_DISC_FEATURE = "IPH_IdentityDisc";

    /**
     * An IPH feature showing up the first time the user is presented with the quieter version of
     * the permission prompt (for notifications).
     */
    String QUIET_NOTIFICATION_PROMPTS_FEATURE = "IPH_QuietNotificationPrompts";

    /** An IPH feature to show on the feed header menu button of the FeedNewTabPage. */
    String FEED_HEADER_MENU_FEATURE = "IPH_FeedHeaderMenu";

    /** An IPH used for web feed awareness to be shown on the NTP for the Web Feed tab. */
    String WEB_FEED_AWARENESS_FEATURE = "IPH_WebFeedAwareness";

    /** An IPH feature to show the first re-engagement notification. */
    String CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE = "IPH_ChromeReengagementNotification1";

    /** An IPH feature to show the second re-engagement notification. */
    String CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE = "IPH_ChromeReengagementNotification2";

    /** An IPH feature to show the third re-engagement notification. */
    String CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE = "IPH_ChromeReengagementNotification3";

    /** An IPH feature to inform users that installing a PWA is an option. */
    String PWA_INSTALL_AVAILABLE_FEATURE = "IPH_PwaInstallAvailableFeature";

    /** An IPH feature to inform about changing permissions in PageInfo. */
    String PAGE_INFO_FEATURE = "IPH_PageInfo";

    /** An IPH feature to inform users about the StoreInfo feature in PageInfo. */
    String PAGE_INFO_STORE_INFO_FEATURE = "IPH_PageInfoStoreInfo";

    /** An IPH feature to inform users about the screenshot sharing feature. */
    String IPH_SHARE_SCREENSHOT_FEATURE = "IPH_ShareScreenshot";

    /** An IPH feature to inform users about the Sharing Hub link toggle. */
    String IPH_SHARING_HUB_LINK_TOGGLE_FEATURE = "IPH_SharingHubLinkToggle";

    /** An IPH feature to inform users about the WebFeed follow feature. */
    String IPH_WEB_FEED_FOLLOW_FEATURE = "IPH_WebFeedFollow";

    /** A dialog IPH feature to inform users about the WebFeed post-follow. */
    String IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE = "IPH_WebFeedPostFollowDialog";

    /** A dialog IPH feature to inform users about the WebFeed post-follow after the UI update. */
    String IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE_WITH_UI_UPDATE =
            "IPH_WebFeedPostFollowDialogWithUIUpdate";

    /** An IPH feature to inform users about the link-to-text on selection share. */
    String SHARED_HIGHLIGHTING_BUILDER_FEATURE = "IPH_SharedHighlightingBuilder";

    /** An IPH feature encouraging users to create highlights. */
    String SHARED_HIGHLIGHTING_RECEIVER_FEATURE = "IPH_SharedHighlightingReceiver";

    /** An IPH feature to inform users about the Webnotes Stylize feature in Sharing Hub. */
    String SHARING_HUB_WEBNOTES_STYLIZE_FEATURE = "IPH_SharingHubWebnotesStylize";

    /** An IPH feature to inform users that a price drop has occurred in any of their open tabs */
    String PRICE_DROP_NTP_FEATURE = "IPH_PriceDropNTP";

    /** An IPH feature to inform users that tabs from another synced device can be restored on FRE. */
    String RESTORE_TABS_ON_FRE_FEATURE = "IPH_RestoreTabsOnFRE";
}
