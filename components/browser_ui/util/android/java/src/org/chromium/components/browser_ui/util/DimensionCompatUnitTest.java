// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Insets;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.view.WindowInsets;
import android.view.WindowMetrics;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.TimeoutException;

/** Unit tests for {@link DimensionCompat}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DimensionCompatUnitTest {
    private Activity mActivity;
    private static final int TEST_SCREEN_WIDTH = 1000;
    private static final int TEST_SCREEN_HEIGHT = 1000;
    private static final int TEST_NAVIGATION_BAR_HEIGHT = 30;

    private static final int TEST_STATUS_BAR_HEIGHT = 30;
    private static WindowInsets sTestSysBarInsets;
    @Mock private Resources mResources;

    @Implements(WindowMetrics.class)
    public static class ShadowWindowMetrics {
        @Implementation
        public static WindowInsets getWindowInsets() {
            return sTestSysBarInsets;
        }
    }

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.setupActivity(Activity.class);
        if (VERSION.SDK_INT >= VERSION_CODES.R) {
            sTestSysBarInsets =
                    new WindowInsets.Builder()
                            .setInsets(
                                    WindowInsets.Type.systemBars(),
                                    Insets.of(
                                            0,
                                            TEST_STATUS_BAR_HEIGHT,
                                            0,
                                            TEST_NAVIGATION_BAR_HEIGHT))
                            .build();
        }
    }

    @Test
    @Config(
            sdk = Build.VERSION_CODES.R,
            shadows = {ShadowWindowMetrics.class},
            qualifiers = "w" + TEST_SCREEN_WIDTH + "dp-h" + TEST_SCREEN_HEIGHT + "dp")
    public void getDimensions() {
        final CallbackHelper helper = new CallbackHelper();
        DimensionCompat dimensionCompat =
                DimensionCompat.create(mActivity, () -> helper.notifyCalled());
        assertEquals(
                "Window height is not as expected.",
                TEST_SCREEN_HEIGHT,
                dimensionCompat.getWindowHeight());
        assertEquals(
                "Window width is not as expected.",
                TEST_SCREEN_WIDTH,
                dimensionCompat.getWindowWidth());
        assertEquals(
                "Nav bar height is not as expected.",
                TEST_NAVIGATION_BAR_HEIGHT,
                dimensionCompat.getNavbarHeight());
        assertEquals(
                "Status bar height is not as expected.",
                TEST_STATUS_BAR_HEIGHT,
                dimensionCompat.getStatusbarHeight());

        try {
            dimensionCompat.updatePosition();
            helper.waitForCallback(0);
        } catch (TimeoutException e) {
            throw new AssertionError("Position updater was not invoked.", e);
        }
    }

    @Test
    @Config(
            sdk = Build.VERSION_CODES.Q,
            shadows = {ShadowWindowMetrics.class},
            qualifiers = "w" + TEST_SCREEN_WIDTH + "dp-h" + TEST_SCREEN_HEIGHT + "dp")
    public void getDimensionsLegacy() {
        Activity mockActivity = spy(mActivity);
        int statusBarHeight = 25;
        when(mockActivity.getResources()).thenReturn(mResources);
        when(mResources.getIdentifier("status_bar_height", "dimen", "android")).thenReturn(1);
        when(mResources.getDimensionPixelSize(anyInt())).thenReturn(statusBarHeight);
        DimensionCompat dimensionCompat = DimensionCompat.create(mockActivity, null);
        assertEquals(
                "Window height is not as expected.",
                TEST_SCREEN_HEIGHT,
                dimensionCompat.getWindowHeight());
        assertEquals(
                "Window width is not as expected.",
                TEST_SCREEN_WIDTH,
                dimensionCompat.getWindowWidth());
        assertEquals("Nav bar height is not as expected.", 0, dimensionCompat.getNavbarHeight());
        assertEquals(
                "Status bar height is not as expected.",
                statusBarHeight,
                dimensionCompat.getStatusbarHeight());
    }
}
