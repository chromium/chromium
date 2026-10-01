// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.UiModeManager;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

/** Unit tests for {@link BrowserUiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_AUTO_PROJECTED)
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
    public void testRecordTabSwitcherButtonClicked_Exit_Ntp() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("NewTabPage.Module.Click").build();

        BrowserUiUtils.recordTabSwitcherButtonClicked(
                /* isExit= */ true, /* isCurrentTabRegularNtp= */ true);

        Assert.assertEquals(1, mUserActionTester.getActionCount("MobileHubExitViaButton"));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordTabSwitcherButtonClicked_Exit_NonNtp() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("NewTabPage.Module.Click").build();

        BrowserUiUtils.recordTabSwitcherButtonClicked(
                /* isExit= */ true, /* isCurrentTabRegularNtp= */ false);

        Assert.assertEquals(1, mUserActionTester.getActionCount("MobileHubExitViaButton"));
        histogramWatcher.assertExpected();
    }

    @Test
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
    public void testRecordTabSwitcherButtonClicked_Enter_NonNtp() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("NewTabPage.Module.Click").build();

        BrowserUiUtils.recordTabSwitcherButtonClicked(
                /* isExit= */ false, /* isCurrentTabRegularNtp= */ false);

        histogramWatcher.assertExpected();
        Assert.assertEquals(0, mUserActionTester.getActionCount("MobileHubExitViaButton"));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_AUTO_PROJECTED)
    @Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
    public void testIsAndroidAutoProjected_FlagDisabled() {
        Context context = createMockContext(Configuration.UI_MODE_TYPE_CAR);
        Assert.assertFalse(BrowserUiUtils.isAndroidAutoProjected(context));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
    public void testIsAndroidAutoProjected_AutomotiveDevice() {
        // DeviceInfo.isAutomotive is true for AAOS but false for AAP.
        DeviceInfo.setIsAutomotiveForTesting(true);
        Context context = createMockContext(Configuration.UI_MODE_TYPE_CAR);
        Assert.assertFalse(BrowserUiUtils.isAndroidAutoProjected(context));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testIsAndroidAutoProjected_UnsupportedSdk_Android16() {
        Context context = createMockContext(Configuration.UI_MODE_TYPE_CAR);
        Assert.assertFalse(BrowserUiUtils.isAndroidAutoProjected(context));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
    public void testIsAndroidAutoProjected_CarUiMode_Android17() {
        Context context = createMockContext(Configuration.UI_MODE_TYPE_CAR);
        Assert.assertTrue(BrowserUiUtils.isAndroidAutoProjected(context));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
    public void testIsAndroidAutoProjected_NormalUiMode_Android17() {
        Context context = createMockContext(Configuration.UI_MODE_TYPE_NORMAL);
        Assert.assertFalse(BrowserUiUtils.isAndroidAutoProjected(context));
    }

    private Context createMockContext(int uiModeType) {
        Context context = mock(Context.class);
        UiModeManager uiModeManager = mock(UiModeManager.class);
        when(context.getSystemService(Context.UI_MODE_SERVICE)).thenReturn(uiModeManager);
        when(uiModeManager.getCurrentModeType()).thenReturn(uiModeType);
        return context;
    }
}
