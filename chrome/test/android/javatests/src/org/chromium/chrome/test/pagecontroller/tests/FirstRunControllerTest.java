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
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.pagecontroller.controllers.first_run.TOSController;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.NewTabPageController;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;

/**
 * Test the First Run Experience.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class FirstRunControllerTest {
    public ChromeUiAutomatorTestRule mUiAutomatorRule = new ChromeUiAutomatorTestRule();
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mUiAutomatorRule);

    @Before
    public void setUp() {
        mChromeUiRule.launchApplication();
    }

    @Test
    public void testFirstRunIsShown() {
        Assert.assertTrue("TOS page should be shown.",
                          TOSController.getInstance().isCurrentPageThis());
    }

    @Switches(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    @Test
    public void testDisableFre() {
        Assert.assertTrue(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE + " should work.",
                          NewTabPageController.getInstance().isCurrentPageThis());
        Assert.assertFalse("TOS Page should not be detected.",
                          TOSController.getInstance().isCurrentPageThis());
    }
}
