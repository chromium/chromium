// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

/** Unit tests for {@link BrowserUiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BrowserUiUtilsUnitTest {
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testRecordTabSwitcherButtonClicked_Exit_Ntp() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("NewTabPage.Module.Click").build();

        BrowserUiUtils.recordTabSwitcherButtonClicked(
                /* isExit= */ true, /* isCurrentTabRegularNtp= */ true);

        Assert.assertEquals(1, mUserActionTester.getActionCount("MobileHubExitViaButton"));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordTabSwitcherButtonClicked_Exit_NonNtp() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("NewTabPage.Module.Click").build();

        BrowserUiUtils.recordTabSwitcherButtonClicked(
                /* isExit= */ true, /* isCurrentTabRegularNtp= */ false);

        Assert.assertEquals(1, mUserActionTester.getActionCount("MobileHubExitViaButton"));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordTabSwitcherButtonClicked_Enter_Ntp() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "NewTabPage.Module.Click",
                                ModuleTypeOnStartAndNtp.TAB_SWITCHER_BUTTON)
                        .build();

        BrowserUiUtils.recordTabSwitcherButtonClicked(
                /* isExit= */ false, /* isCurrentTabRegularNtp= */ true);

        histogramWatcher.assertExpected();
        Assert.assertEquals(0, mUserActionTester.getActionCount("MobileHubExitViaButton"));
    }

    @Test
    @SmallTest
    public void testRecordTabSwitcherButtonClicked_Enter_NonNtp() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("NewTabPage.Module.Click").build();

        BrowserUiUtils.recordTabSwitcherButtonClicked(
                /* isExit= */ false, /* isCurrentTabRegularNtp= */ false);

        histogramWatcher.assertExpected();
        Assert.assertEquals(0, mUserActionTester.getActionCount("MobileHubExitViaButton"));
    }
}
