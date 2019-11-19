// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.tabswitcher;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.NewTabPageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiLocationException;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Tab Switcher Page Controller.
 */
public class TabSwitcherController extends PageController {
    private static final Pattern PATTERN_NUMBER_OF_OPEN_TABS = Pattern.compile("^(\\d+) .*");
    private static final IUi2Locator LOCATOR_CLOSE_ALL_TABS =
            Ui2Locators.withAnyResEntry(R.id.close_all_tabs_button);
    private static final IUi2Locator LOCATOR_NEW_TAB =
            Ui2Locators.withAnyResEntry(R.id.tab_switcher_new_tab_button, R.id.new_tab_button);
    private static final IUi2Locator LOCATOR_TAB_SWITCHER_BUTTON = Ui2Locators.withAnyResEntry(
            R.id.tab_switcher_button, R.id.tab_switcher_mode_tab_switcher_button);
    private static final IUi2Locator LOCATOR_MENU = Ui2Locators.withAnyResEntry(R.id.menu_button);

    private static final TabSwitcherController sInstance = new TabSwitcherController();

    public static TabSwitcherController getInstance() {
        return sInstance;
    }

    private TabSwitcherController() {}

    public void clickCloseAllTabs() {
        // Default to  use the close all tabs button.
        if (mLocatorHelper.isOnScreen(LOCATOR_CLOSE_ALL_TABS)) {
            mUtils.click(LOCATOR_CLOSE_ALL_TABS);
        } else {
            // If it's not found for whatever reason, then do it through the menu.
            clickMenu().clickCloseAllTabs();
        }
    }

    public void clickTabSwitcher() {
        mUtils.click(LOCATOR_TAB_SWITCHER_BUTTON);
    }

    public int getNumberOfOpenTabs() {
        String text = mLocatorHelper.getOneDescription(LOCATOR_TAB_SWITCHER_BUTTON);
        Matcher matcher = PATTERN_NUMBER_OF_OPEN_TABS.matcher(text);
        if (matcher.matches()) {
            return Integer.valueOf(matcher.group(1));
        } else {
            throw new UiLocationException(
                    "Could not get number of open tabs in Tab Switcher button.",
                    LOCATOR_TAB_SWITCHER_BUTTON);
        }
    }
    public NewTabPageController clickNewTab() {
        mUtils.click(LOCATOR_NEW_TAB);
        return NewTabPageController.getInstance().verifyActive();
    }

    public TabSwitcherMenuController clickMenu() {
        mUtils.click(LOCATOR_MENU);
        return TabSwitcherMenuController.getInstance().verifyActive();
    }

    @Override
    public TabSwitcherController verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_NEW_TAB);
        return this;
    }
}
