// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.pagecontroller.controllers.first_run.TOSController;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.NewTabPageController;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;

/**
 * Test the First Run Experience pre-MICe. The MICe FRE flow is covered by the test
 * {@link FirstRunActivitySigninAndSyncTest}.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
public class FirstRunControllerTest {
    public ChromeUiAutomatorTestRule mUiAutomatorRule = new ChromeUiAutomatorTestRule();
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mUiAutomatorRule);

    @Rule
    public TestRule mCommandLineFlagsRule = CommandLineFlags.getTestRule();

    @Before
    public void setUp() {
        mChromeUiRule.launchApplication();
    }

    @DisabledTest(message = "https://crbug.com/1286635")
    @Test
    public void testFirstRunIsShown() {
        Assert.assertTrue("TOS page should be shown.",
                          TOSController.getInstance().isCurrentPageThis());
    }

    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    @Test
    public void testDisableFre() {
        Assert.assertTrue(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE + " should work.",
                          NewTabPageController.getInstance().isCurrentPageThis());
        Assert.assertFalse("TOS Page should not be detected.",
                          TOSController.getInstance().isCurrentPageThis());
    }
}
