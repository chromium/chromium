// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.controllers.tabswitcher.TabSwitcherController;
import org.chromium.chrome.test.pagecontroller.controllers.urlpage.UrlPage;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * New Tab Page Page Controller, handles either Feed or Zine implementations.
 */
public class NewTabPageController extends PageController {
    private static final float SCROLL_SWIPE_FRACTION = 0.6f;
    // The test will timeout for Small and Medium sizes before 50 swipes is
    // reached.  This number is set so that it should not be hit if everything
    // is working fine.  Please bump it up if test cases exceeding 50 swipes
    // becomes common.
    private static final int MAX_SCROLL_SWIPES = 50;

    private static final IUi2Locator LOCATOR_SEARCH_BOX_TEXT =
            Ui2Locators.withAnyResEntry(R.id.search_box_text);
    private static final IUi2Locator LOCATOR_URL_BAR = Ui2Locators.withAnyResEntry(R.id.url_bar);
    private static final IUi2Locator LOCATOR_SEARCH_PROVIDER_LOGO =
            Ui2Locators.withAnyResEntry(R.id.search_provider_logo);
    private static final IUi2Locator LOCATOR_MORE_BUTTON =
            Ui2Locators.withAnyResEntry(R.id.action_button);
    private static final IUi2Locator LOCATOR_MENU_BUTTON =
            Ui2Locators.withAnyResEntry(R.id.menu_button);
    private static final IUi2Locator LOCATOR_FEED_STREAM_RECYCLER_VIEW =
            Ui2Locators.withAnyResEntry(R.id.feed_stream_recycler_view);

    private static final IUi2Locator LOCATOR_TAB_SWITCHER =
            Ui2Locators.withAnyResEntry(R.id.tab_switcher_button);

    private static final IUi2Locator LOCATOR_NEW_TAB_PAGE = Ui2Locators.withAnyResEntry(
            R.id.ntp_content, R.id.feed_stream_recycler_view, R.id.feed_content_card);

    private static final NewTabPageController sInstance = new NewTabPageController();
    private NewTabPageController() {}
    public static NewTabPageController getInstance() {
        return sInstance;
    }

    public void scrollTowardsTop(float screenHeightPercentage) {
        mUtils.swipeDownVertically(screenHeightPercentage);
    }

    public void scrollTowardsBottom(float screenHeightPercentage) {
        mUtils.swipeUpVertically(screenHeightPercentage);
    }


    public boolean hasScrolledToTop() {
        return mLocatorHelper.isOnScreen(LOCATOR_SEARCH_PROVIDER_LOGO);
    }

    public void scrollToTop() {
        scrollToTop(MAX_SCROLL_SWIPES);
    }

    public void scrollToTop(int maxSwipes) {
        for (int swipes = 0; swipes < maxSwipes && !hasScrolledToTop(); swipes++) {
            scrollTowardsTop(SCROLL_SWIPE_FRACTION);
        }
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
