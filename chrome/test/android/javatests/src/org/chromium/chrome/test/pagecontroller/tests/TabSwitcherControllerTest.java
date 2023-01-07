// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
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
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1,
            message = "https://crbug.com/1130617 https://crbug.com/1141179")
    public void
    testOpenNewTab() {
        mController.clickNewTab();
        Assert.assertTrue(NewTabPageController.getInstance().isCurrentPageThis());
    }

    private void waitForTabCount(final int count) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(mController.getNumberOfOpenTabs(), Matchers.is(count));
        });
    }

    @Test
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1,
            message = "https://crbug.com/1130617 https://crbug.com/1141179")
    public void
    testCloseAllTabs() {
        mController.clickNewTab().openTabSwitcher().clickNewTab().openTabSwitcher();
        mController.clickCloseAllTabs();
        waitForTabCount(0);
    }

    @Test
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1,
            message = "https://crbug.com/1130617 https://crbug.com/1141179")
    public void
    testNumberOfOpenTabs() {
        int startTabs = mController.getNumberOfOpenTabs();
        mController.clickNewTab().openTabSwitcher();
        waitForTabCount(startTabs + 1);
    }

    @Test
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1,
            message = "https://crbug.com/1130617 https://crbug.com/1141179")
    public void
    testClickTabSwitcher() {
        mController.clickTabSwitcher();
        Assert.assertTrue(NewTabPageController.getInstance().isCurrentPageThis());
    }

    @Test
    @DisabledTest(message = "https://crbug.com/1140998")
    public void testOpenMenu() {
        mController.clickMenu();
        Assert.assertTrue(TabSwitcherMenuController.getInstance().isCurrentPageThis());
    }
}
