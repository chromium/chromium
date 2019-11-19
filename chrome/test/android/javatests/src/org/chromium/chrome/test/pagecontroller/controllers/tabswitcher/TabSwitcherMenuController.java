// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.tabswitcher;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Tab Switcher Menu Dialog Page Controller.
 */
public class TabSwitcherMenuController extends PageController {
    private static final IUi2Locator LOCATOR_MENU_BOX =
            Ui2Locators.withAnyResEntry(R.id.app_menu_list);

    // TODO(aluo): Add resource ids for menus, using text will break when switching languages
    private static final IUi2Locator LOCATOR_MENU =
            Ui2Locators.withPath(LOCATOR_MENU_BOX, Ui2Locators.withText("Close all tabs"));

    private static final IUi2Locator LOCATOR_NEW_TAB =
            Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.menu_item_text),
                    Ui2Locators.withTextString(R.string.button_new_tab));
    private static final IUi2Locator LOCATOR_NEW_INCOGNITO_TAB =
            Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.menu_item_text),
                    Ui2Locators.withTextString(R.string.button_new_incognito_tab));
    private static final IUi2Locator LOCATOR_CLOSE_ALL_TABS =
            Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.menu_item_text),
                    Ui2Locators.withTextString(R.string.menu_close_all_tabs));
    private static final IUi2Locator LOCATOR_SETTINGS =
            Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.menu_item_text),
                    Ui2Locators.withTextString(R.string.menu_preferences));

    private static final TabSwitcherMenuController sInstance = new TabSwitcherMenuController();
    public static TabSwitcherMenuController getInstance() {
        return sInstance;
    }

    @Override
    public TabSwitcherMenuController verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_MENU);
        return this;
    }

    public void clickNewTab() {
        mUtils.click(LOCATOR_NEW_TAB);
    }

    public void clickNewIncognitoTab() {
        mUtils.click(LOCATOR_NEW_INCOGNITO_TAB);
    }

    public void clickCloseAllTabs() {
        mUtils.click(LOCATOR_CLOSE_ALL_TABS);
    }

    public void clickSettings() {
        mUtils.click(LOCATOR_SETTINGS);
    }

    public TabSwitcherController dismiss() {
        mUtils.clickOutsideOf(LOCATOR_MENU_BOX);
        return TabSwitcherController.getInstance().verifyActive();
    }
}
