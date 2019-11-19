// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Chrome Menu (NTP 3-dot menu) Page Controller.
 */
public class ChromeMenu extends PageController {
    private static final IUi2Locator LOCATOR_CHROME_MENU_BOX =
            Ui2Locators.withAnyResEntry(AppMenuTestSupport.getAppMenuLayoutListViewId());

    private static final IUi2Locator LOCATOR_NEW_TAB = Ui2Locators.withPath(
            Ui2Locators.withAnyResEntry(AppMenuTestSupport.getStandardMenuItemTextViewId()),
            Ui2Locators.withTextString(R.string.menu_new_tab));
    private static final IUi2Locator LOCATOR_NEW_INCOGNITO_TAB = Ui2Locators.withPath(
            Ui2Locators.withAnyResEntry(AppMenuTestSupport.getStandardMenuItemTextViewId()),
            Ui2Locators.withTextString(R.string.menu_new_incognito_tab));

    private static final ChromeMenu sInstance = new ChromeMenu();
    private ChromeMenu() {}
    public static ChromeMenu getInstance() {
        return sInstance;
    }

    @Override
    public ChromeMenu verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_CHROME_MENU_BOX);
        return this;
    }

    public NewTabPageController openNewTab() {
        mUtils.click(LOCATOR_NEW_TAB);
        return NewTabPageController.getInstance().verifyActive();
    }

    public IncognitoNewTabPageController openNewIncognitoTab() {
        mUtils.click(LOCATOR_NEW_INCOGNITO_TAB);
        return IncognitoNewTabPageController.getInstance().verifyActive();
    }

    public NewTabPageController dismiss() {
        mUtils.clickOutsideOf(LOCATOR_CHROME_MENU_BOX);
        return NewTabPageController.getInstance().verifyActive();
    }
}
