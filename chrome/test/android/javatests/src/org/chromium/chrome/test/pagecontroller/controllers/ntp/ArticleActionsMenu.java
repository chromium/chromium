// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import com.google.android.libraries.feed.sharedstream.contextmenumanager.R;

import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.controllers.urlpage.UrlPage;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Article Actions Menu (long-press on NTP article) Page Controller.
 */
public class ArticleActionsMenu extends PageController {
    private static final IUi2Locator LOCATOR_MENU = Ui2Locators.withClassRegex(".*ListView");

    private static final IUi2Locator LOCATOR_OPEN_NEW_TAB = Ui2Locators.withIndex(0,
            Ui2Locators.withAnyResEntry(
                    org.chromium.chrome.R.id.title, R.id.feed_simple_list_item));
    private static final IUi2Locator LOCATOR_OPEN_INCOGNITO = Ui2Locators.withIndex(1,
            Ui2Locators.withAnyResEntry(
                    org.chromium.chrome.R.id.title, R.id.feed_simple_list_item));
    private static final IUi2Locator LOCATOR_DOWNLOAD_LINK = Ui2Locators.withIndex(2,
            Ui2Locators.withAnyResEntry(
                    org.chromium.chrome.R.id.title, R.id.feed_simple_list_item));
    private static final IUi2Locator LOCATOR_REMOVE = Ui2Locators.withIndex(3,
            Ui2Locators.withAnyResEntry(
                    org.chromium.chrome.R.id.title, R.id.feed_simple_list_item));
    private static final IUi2Locator LOCATOR_LEARN_MORE = Ui2Locators.withIndex(4,
            Ui2Locators.withAnyResEntry(
                    org.chromium.chrome.R.id.title, R.id.feed_simple_list_item));

    private static ArticleActionsMenu sInstance = new ArticleActionsMenu();
    private ArticleActionsMenu() {}
    public static ArticleActionsMenu getInstance() {
        return sInstance;
    }

    @Override
    public ArticleActionsMenu verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_LEARN_MORE);
        return this;
    }

    public UrlPage clickOpenNewTab() {
        mUtils.click(LOCATOR_OPEN_NEW_TAB);
        return UrlPage.getInstance().verifyActive();
    }

    public UrlPage clickOpenIncognitoTab() {
        mUtils.click(LOCATOR_OPEN_INCOGNITO);
        return UrlPage.getInstance().verifyActive();
    }

    public void clickDownloadLink() {
        mUtils.click(LOCATOR_DOWNLOAD_LINK);
    }

    public NewTabPageController clickRemoveArticle() {
        mUtils.click(LOCATOR_REMOVE);
        return NewTabPageController.getInstance().verifyActive();
    }

    public void clickLearnMore() {
        mUtils.click(LOCATOR_LEARN_MORE);
    }

    public NewTabPageController dismiss() {
        mUtils.clickOutsideOf(LOCATOR_MENU);
        return NewTabPageController.getInstance().verifyActive();
    }
}
