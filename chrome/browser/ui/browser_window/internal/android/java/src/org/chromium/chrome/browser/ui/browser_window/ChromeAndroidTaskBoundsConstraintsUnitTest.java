// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_FULL_SCREEN_BOUNDS_IN_PX;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_MAX_TAPPABLE_INSETS_IN_PX;

import android.annotation.SuppressLint;
import android.graphics.Rect;
import android.os.Build;
import android.view.WindowManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
@SuppressLint("NewApi" /* @Config already specifies the required SDK */)
public class ChromeAndroidTaskBoundsConstraintsUnitTest {

    private static final float TEST_DIP_SCALE = 2.0f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DisplayAndroid mMockDisplayAndroid;
    @Mock private WindowManager mMockWindowManager;

    @Before
    public void setUp() {
        ChromeAndroidTaskUnitTestSupport.mockMaxWindowMetrics(
                mMockWindowManager,
                DEFAULT_FULL_SCREEN_BOUNDS_IN_PX,
                DEFAULT_MAX_TAPPABLE_INSETS_IN_PX);
        when(mMockDisplayAndroid.getDipScale()).thenReturn(TEST_DIP_SCALE);
    }

    @Test
    public void apply_clampsInputBoundsThatAreTooLarge() {
        // Arrange: create a Rect that's larger than the maximized size.
        Rect inputBoundsInPx = new Rect(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX);
        inputBoundsInPx.offset(/* dx= */ 0, /* dy= */ 10);

        // Act.
        Rect adjustedBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.apply(
                        inputBoundsInPx, mMockDisplayAndroid, mMockWindowManager);

        // Assert.
        assertEquals(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, adjustedBoundsInPx);
    }

    @Test
    public void apply_clampsInputBoundsThatAreTooSmall() {
        // Arrange: create a Rect that's smaller than the smallest size.
        int minSizeInPx =
                DisplayUtil.dpToPx(
                        mMockDisplayAndroid,
                        ChromeAndroidTaskBoundsConstraints.MINIMAL_TASK_SIZE_DP);
        int leftInPx = DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX.centerX();
        int topInPx = DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX.centerY();
        Rect inputBoundsInPx =
                new Rect(
                        leftInPx,
                        topInPx,
                        /* right= */ leftInPx + minSizeInPx - 10,
                        /* bottom= */ topInPx + minSizeInPx - 10);

        // Act.
        Rect adjustedBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.apply(
                        inputBoundsInPx, mMockDisplayAndroid, mMockWindowManager);

        // Assert.
        Rect expectedBoundsInPx =
                new Rect(
                        leftInPx,
                        topInPx,
                        /* right= */ leftInPx + minSizeInPx,
                        /* bottom= */ topInPx + minSizeInPx);
        assertEquals(expectedBoundsInPx, adjustedBoundsInPx);
    }

    @Test
    public void getMaxBoundsInPx() {
        // Act.
        Rect maxBounds = ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(mMockWindowManager);

        // Assert.
        assertEquals(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, maxBounds);
    }
}
