// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.controllers.tabswitcher.TabSwitcherController;
import org.chromium.chrome.test.pagecontroller.controllers.urlpage.UrlPage;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

import java.util.List;

/**
 * New Tab Page Page Controller, handles either Feed or Zine implementations.
 */
public class NewTabPageController extends PageController {
    private static final float SCROLL_SWIPE_FRACTION = 0.6f;
    private static final float MAX_LOAD_ARTICLES_WAIT = 5000f;
    // The test will timeout for Small and Medium sizes before 50 swipes is
    // reached.  This number is set so that it should not be hit if everything
    // is working fine.  Please bump it up if test cases exceeding 50 swipes
    // becomes common.
    private static final int MAX_SCROLL_SWIPES = 50;

    private static final String TEXT_HEADER_STATUS_HIDE = "Hide";
    private static final String TEXT_HEADER_STATUS_SHOW = "Show";
    private static final String REGEX_TEXT_HEADER_STATUS = "^Hide|Show$";

    private static final IUi2Locator LOCATOR_SEARCH_BOX_TEXT =
            Ui2Locators.withAnyResEntry(R.id.search_box_text);
    private static final IUi2Locator LOCATOR_URL_BAR = Ui2Locators.withAnyResEntry(R.id.url_bar);
    private static final IUi2Locator LOCATOR_SEARCH_PROVIDER_LOGO =
            Ui2Locators.withAnyResEntry(R.id.search_provider_logo);
    private static final IUi2Locator LOCATOR_MORE_BUTTON =
            Ui2Locators.withAnyResEntry(R.id.action_button);
    private static final IUi2Locator LOCATOR_MENU_BUTTON =
            Ui2Locators.withAnyResEntry(R.id.menu_button);
    private static final IUi2Locator LOCATOR_BOTTOM_OF_PAGE =
            Ui2Locators.withAnyResEntry(R.id.progress_indicator, R.id.action_button);
    private static final IUi2Locator LOCATOR_FEED_STREAM_RECYCLER_VIEW =
            Ui2Locators.withAnyResEntry(
                    com.google.android.libraries.feed.basicstream.R.id.feed_stream_recycler_view);
    private static final IUi2Locator LOCATOR_HEADER_STATUS =
            Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.header_status),
                    Ui2Locators.withTextRegex(REGEX_TEXT_HEADER_STATUS));
    private static final IUi2Locator LOCATOR_INFO_BAR_MESSAGE =
            Ui2Locators.withAnyResEntry(R.id.infobar_message, R.id.snackbar_message);
    private static final IUi2Locator LOCATOR_INFO_BAR_CLOSE =
            Ui2Locators.withAnyResEntry(R.id.infobar_close_button);

    private static final IUi2Locator LOCATOR_TAB_SWITCHER =
            Ui2Locators.withAnyResEntry(R.id.tab_switcher_button);

    private static final IUi2Locator LOCATOR_NEW_TAB_PAGE =
            Ui2Locators.withAnyResEntry(R.id.ntp_content,
                    com.google.android.libraries.feed.basicstream.R.id.feed_stream_recycler_view,
                    R.id.card_contents);

    private ArticleCardController mAriticleCardController;
    private SuggestionTileController mSuggestionsTileController;

    private static final NewTabPageController sInstance = new NewTabPageController();
    private NewTabPageController() {
        mAriticleCardController = ArticleCardController.getInstance();
        mSuggestionsTileController = SuggestionTileController.getInstance();
    }
    public static NewTabPageController getInstance() {
        return sInstance;
    }

    public ArticleCardController.ImplementationType getArticleImplementationType() {
        if (mLocatorHelper.isOnScreen(LOCATOR_FEED_STREAM_RECYCLER_VIEW)) {
            return ArticleCardController.ImplementationType.FEED;
        } else {
            return ArticleCardController.ImplementationType.ZINE;
        }
    }

    /**
     * Hide articles if shown, and vice-versa.  This will cause page to scroll to where the
     * show/hide articles button is visible.
     */
    public void toggleHideArticles() {
        scrollToTop();
        mUtils.swipeUpVerticallyUntilFound(LOCATOR_HEADER_STATUS, LOCATOR_MORE_BUTTON);
        mUtils.click(LOCATOR_HEADER_STATUS);
    }

    /**
     * This will cause page to scroll to where the show/hide articles button is visible.
     * @return True if articles are currently hidden as indicated by the presence of the
     *         show/hide articles button, else false.
     */
    public boolean areArticlesHidden() {
        scrollToTop();

        String status_text = mLocatorHelper.getOneText(LOCATOR_HEADER_STATUS);

        // The ariticles are hidden if "Show" is displayed, appears to be reversed but makes UI
        // sense since the user can click on "Show" to unhide them.
        if (status_text.equals(TEXT_HEADER_STATUS_HIDE)) {
            return false;
        } else if (status_text.equals(TEXT_HEADER_STATUS_SHOW)) {
            return true;
        } else {
            throw new IllegalStateException("Bad status text: " + status_text);
        }
    }

    public void scrollTowardsTop(float screenHeightPercentage) {
        mUtils.swipeDownVertically(screenHeightPercentage);
    }

    public void scrollTowardsBottom(float screenHeightPercentage) {
        mUtils.swipeUpVertically(screenHeightPercentage);
    }

    public void clickInfoBarMessage() {
        mUtils.click(LOCATOR_INFO_BAR_MESSAGE);
    }

    public String getInfoBarMessage() {
        return mLocatorHelper.getOneText(LOCATOR_INFO_BAR_MESSAGE);
    }

    public NewTabPageController closeInfoBar() {
        mUtils.click(LOCATOR_INFO_BAR_CLOSE);
        return this;
    }

    public boolean hasScrolledToBottom() {
        // If TEXT_HEADER_STATUS_SHOW is displayed, it means articles are hidden.
        IUi2Locator locator = Ui2Locators.withPath(
                LOCATOR_HEADER_STATUS, Ui2Locators.withTextRegex(TEXT_HEADER_STATUS_SHOW));
        if (mLocatorHelper.isOnScreen(locator)) {
            return true;
        } else {
            return mLocatorHelper.isOnScreen(LOCATOR_MORE_BUTTON);
        }
    }

    public boolean hasScrolledToTop() {
        return mLocatorHelper.isOnScreen(LOCATOR_SEARCH_PROVIDER_LOGO);
    }

    public void scrollToBottom() {
        scrollToBottom(MAX_SCROLL_SWIPES);
    }

    public void scrollToBottom(int maxSwipes) {
        int swipes = 0;
        while (swipes++ < maxSwipes && !hasScrolledToBottom()) {
            scrollTowardsBottom(SCROLL_SWIPE_FRACTION);
        }
    }

    public void scrollToTop() {
        scrollToTop(MAX_SCROLL_SWIPES);
    }

    public void scrollToTop(int maxSwipes) {
        for (int swipes = 0; swipes < maxSwipes && !hasScrolledToTop(); swipes++) {
            scrollTowardsTop(SCROLL_SWIPE_FRACTION);
        }
    }

    public List<ArticleCardController.Info> getAllLoadedArticles() {
        return getAllLoadedArticles(getArticleImplementationType());
    }

    /**
     * Get all suggestion tiles.  This will cause the page to scroll to the top.
     * @return List of suggestion infos, possibly empty.
     */
    public List<SuggestionTileController.Info> getAllSuggestionTiles() {
        // Suggestion tiles are currently at the top of the NTP, so need to ensure page is
        // scrolled to the top, otherwise they may be offscreen.
        scrollToTop();
        return mSuggestionsTileController.parseScreen();
    }

    /**
     * Get all loaded articles.  This will cause the page to scroll to top then down to the bottom.
     * @param implementationType The article implementation type, FEED or ZINE.
     * @return                      List of article card infos, this is a list to preserve the
     *                              order of the articles as they appeared on the screen.
     */
    public List<ArticleCardController.Info> getAllLoadedArticles(
            ArticleCardController.ImplementationType implementationType) {
        scrollToTop();
        List<ArticleCardController.Info> allArticles =
                mAriticleCardController.parseScreenForArticles(implementationType);
        do {
            scrollTowardsBottom(SCROLL_SWIPE_FRACTION);
            List<ArticleCardController.Info> currentArticles =
                    mAriticleCardController.parseScreenForArticles(implementationType);
            for (ArticleCardController.Info article : currentArticles) {
                if (!allArticles.contains(article)) {
                    allArticles.add(article);
                }
            }
        } while (!hasScrolledToBottom());

        return allArticles;
    }

    /**
     * Perform the default card action by tapping on it.  This will cause the page to scroll to top
     * then down to where the article is located (or hit bottom if it isn't found).
     * @param article The article info.
     * @return        UrlPage Controller where the article will be loaded.
     */
    public UrlPage clickArticle(ArticleCardController.Info article) {
        scrollToTop();
        IUi2Locator locator = ArticleCardController.getInstance().getLocator(article);
        mUtils.swipeUpVerticallyUntilFound(locator, LOCATOR_BOTTOM_OF_PAGE);
        mUtils.click(locator);
        return UrlPage.getInstance().verifyActive();
    }

    /**
     * The default action is the one that gets performed when the user taps on the tile icon
     * (opens site in a new page).  If user long taps, then a menu is shown providing more choices
     * (not yet implemented).  This will cause the page to scroll to the top.
     */
    public UrlPage clickSuggestionTile(SuggestionTileController.Info tile) {
        scrollToTop();
        IUi2Locator locator = mSuggestionsTileController.getLocator(tile);
        mUtils.swipeUpVerticallyUntilFound(locator, LOCATOR_BOTTOM_OF_PAGE);
        mUtils.click(locator);
        return UrlPage.getInstance().verifyActive();
    }

    /**
     * Click the load more aritcles button at the bottom of the page.  This will cause the page
     * to scroll to the bottom.
     */
    public void clickLoadMoreArticles() {
        scrollToBottom();
        mUtils.click(LOCATOR_MORE_BUTTON);
    }

    /**
     * Open the tab switcher at the top.  This will cause the page to scroll to the top.
     * @return The TabSwitcher Page Controller.
     */
    public TabSwitcherController openTabSwitcher() {
        scrollToTop();
        mUtils.click(LOCATOR_TAB_SWITCHER);
        return TabSwitcherController.getInstance().verifyActive();
    }

    /**
     * Open the 3-dot menu at the top.  This will cause the page to scroll to the top.
     * @return The ChromeMenu Page Controller.
     */
    public ChromeMenu openChromeMenu() {
        scrollToTop();
        mUtils.click(LOCATOR_MENU_BUTTON);
        return ChromeMenu.getInstance().verifyActive();
    }

    public ArticleActionsMenu openArticleContextMenu(ArticleCardController.Info card) {
        scrollToTop();
        IUi2Locator locator = mAriticleCardController.getLocator(card);
        mUtils.swipeUpVerticallyUntilFound(locator, LOCATOR_BOTTOM_OF_PAGE);
        mUtils.longClick(locator);
        return ArticleActionsMenu.getInstance().verifyActive();
    }

    public UrlPage omniboxSearch(String url) {
        mUtils.click(LOCATOR_SEARCH_BOX_TEXT);
        mUtils.setTextAndEnter(LOCATOR_URL_BAR, url);
        return UrlPage.getInstance().verifyActive();
    }

    @Override
    public NewTabPageController verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_NEW_TAB_PAGE);
        return this;
    }
}
