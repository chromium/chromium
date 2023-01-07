// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests;

import static org.junit.Assert.assertTrue;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.ChromeMenu;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.NewTabPageController;
import org.chromium.chrome.test.pagecontroller.controllers.urlpage.UrlPage;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;

/**
 * Tests for the NewTabPageController.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class NewTabPageControllerTest {
    public ChromeUiAutomatorTestRule mRule = new ChromeUiAutomatorTestRule();

    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mRule);

    private NewTabPageController mController;

    @Before
    public void setUp() {
        mController = mChromeUiRule.launchIntoNewTabPageOnFirstRun();
    }

    @Test
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1,
            message = "https://crbug.com/1130617 https://crbug.com/1141179")
    public void
    testIsCurrentPageThis() {
        Assert.assertTrue(mController.isCurrentPageThis());
    }

    @LargeTest
    @Test
    @DisableIf.
    Build(sdk_is_greater_than = VERSION_CODES.O_MR1, message = "https://crbug.com/1130617")
    public void testScrollPage() {
        mController.scrollToTop();
        assertTrue(mController.hasScrolledToTop());
    }

    @Test
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1,
            message = "https://crbug.com/1130617 https://crbug.com/1141179")
    public void
    testOpenChromeMenu() {
        ChromeMenu menu = mController.openChromeMenu();
        Assert.assertTrue(menu.isCurrentPageThis());
    }

    @Test
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1,
            message = "https://crbug.com/1130617 https://crbug.com/1141179")
    public void
    testOmniboxSearch() {
        UrlPage urlPage = mController.omniboxSearch("www.google.com");
        Assert.assertTrue(urlPage.isCurrentPageThis());
    }
}
