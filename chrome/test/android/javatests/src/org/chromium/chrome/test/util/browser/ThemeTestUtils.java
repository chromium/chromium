// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.annotation.TargetApi;
import android.os.Build;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Utility methods for tests which customize the tab's theme color.
 */
public class ThemeTestUtils {
    public static int getDefaultThemeColor(Tab tab) {
        return ChromeColors.getDefaultThemeColor(
                tab.getContext().getResources(), tab.isIncognito());
    }

    /**
     * Waits for the activity active tab's theme-color to change to the passed-in color.
     */
    public static void waitForThemeColor(ChromeActivity activity, int expectedColor)
            throws ExecutionException, TimeoutException {
        waitForThemeColor(activity, expectedColor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    public static void waitForThemeColor(ChromeActivity activity, int expectedColor, long timeoutMs)
            throws ExecutionException, TimeoutException {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activity.getRootUiCoordinatorForTesting()
                                       .getTopUiThemeColorProvider()
                                       .getThemeColor(),
                    Matchers.is(expectedColor));
        }, timeoutMs, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Asserts that the status bar color equals the passed-in color.
     * Method is for Android L+ because it relies on Window#getStatusBarColor() which was introduced
     * in L.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
    public static void assertStatusBarColor(ChromeActivity activity, int expectedColor) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            expectedColor = ColorUtils.getDarkenedColorForStatusBar(expectedColor);
        }
        Assert.assertEquals(expectedColor, activity.getWindow().getStatusBarColor());
    }
}
