// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests.codelab;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.ChromeMenu;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;

/**
 * Test for Page Controller Code Lab.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class SettingsForCodelabTest {
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

    private ChromeMenu mChromeMenu;

    @Before
    public void setUp() {
        // TODO: Obtain a ChromeMenu instance.  Hint, start with
        //       mChromeUiRule.launchIntoNewTabPageOnFirstRun().
        mChromeMenu = null;
    }

    @Test
    public void testOpenCodelabSettings() {
        // TODO: Uncomment and add a method to ChromeMenu that returns an
        // instance of SettingsControllerForCodelab to pass this test.

        // mChromeMenu.openSettingsForCodelab();
    }

    @Test
    public void testSwitchSearchEngine() {
        // TODO: Uncomment and implement clickSearchEngine that verifies the
        // page has changed to the default search engine selection page.

        // SearchEngineSelectionControllerForCodelab engineSelection =
        //         mChromeMenu.openSettingsForCodelab().clickSearchEngine();
        // Assert.assertEquals(engineSelection.getEngineChoice(), "Google");

        // TODO: Change the search engine to something else, then verify that
        // the change is reflected in the UI.
    }
}
