// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * An example test that demonstrates how to use Page Controllers.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class ExampleTest {
    // ChromeUiAutomatorTestRule will capture a screen shot and UI Hierarchy info in the event
    // of a test failure to aid test debugging.
    public ChromeUiAutomatorTestRule mUiAutomatorRule = new ChromeUiAutomatorTestRule();

    // ChromeUiApplicationTestRule provides a way to launch the Chrome Application under test
    // and access to the Page Controllers.
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    // The rule chain allows deterministic ordering of test rules so that the
    // UiAutomatorRule will print debugging information in case of errors
    // before the application is shut-down.
    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mUiAutomatorRule);

    @Before
    public void setUp() {
        // Do any common setup here.
        mChromeUiRule.launchApplication();
    }

    @After
    public void tearDown() {
        // Do any common cleanup here.
    }

    @Test
    public void testPageFound() {
        PageController controller = new PageController() {
            @Override
            public PageController verifyActive() {
                IUi2Locator packageLocator =
                        Ui2Locators.withPackageName(mChromeUiRule.getApplicationPackage());
                mLocatorHelper.getOne(packageLocator);
                return this;
            }
        };
        Assert.assertTrue("Application should have loaded", controller.isCurrentPageThis());
    }

    @Test
    public void testPageNotFound() {
        PageController controller = new PageController() {
            @Override
            public PageController verifyActive() {
                IUi2Locator packageLocator = Ui2Locators.withPackageName("wrong.package.name");
                mLocatorHelper.getOne(packageLocator);
                return this;
            }
        };
        Assert.assertFalse(
                "Wrong package should not have been detected", controller.isCurrentPageThis());
    }
}
