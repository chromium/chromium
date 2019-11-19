// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.NewTabPageController;
import org.chromium.chrome.test.pagecontroller.controllers.tabswitcher.TabSwitcherController;
import org.chromium.chrome.test.pagecontroller.controllers.tabswitcher.TabSwitcherMenuController;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;

/**
 * Tests for the TabSwitcherController.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class TabSwitcherControllerTest {
    public ChromeUiAutomatorTestRule mRule = new ChromeUiAutomatorTestRule();

    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mRule);

    private TabSwitcherController mController;

    @Before
    public void setUp() {
        mController = mChromeUiRule.launchIntoNewTabPageOnFirstRun().openTabSwitcher();
    }

    @Test
    public void testOpenNewTab() {
        mController.clickNewTab();
        Assert.assertTrue(NewTabPageController.getInstance().isCurrentPageThis());
    }

    @Test
    public void testCloseAllTabs() {
        mController.clickNewTab().openTabSwitcher().clickNewTab().openTabSwitcher();
        mController.clickCloseAllTabs();
        Assert.assertEquals(0, mController.getNumberOfOpenTabs());
    }

    @Test
    public void testNumberOfOpenTabs() {
        int startTabs = mController.getNumberOfOpenTabs();
        mController.clickNewTab().openTabSwitcher();
        Assert.assertEquals(startTabs + 1, mController.getNumberOfOpenTabs());
    }

    @Test
    public void testClickTabSwitcher() {
        mController.clickTabSwitcher();
        Assert.assertTrue(NewTabPageController.getInstance().isCurrentPageThis());
    }

    @Test
    public void testOpenMenu() {
        mController.clickMenu();
        Assert.assertTrue(TabSwitcherMenuController.getInstance().isCurrentPageThis());
    }
}
