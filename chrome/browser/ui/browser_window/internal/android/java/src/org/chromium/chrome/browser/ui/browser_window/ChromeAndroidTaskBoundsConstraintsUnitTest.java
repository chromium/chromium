// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_FULL_SCREEN_BOUNDS_IN_PX;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_MAX_TAPPABLE_INSETS_IN_PX;

import android.graphics.Rect;
import android.os.Build;
import android.view.WindowManager;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
public class ChromeAndroidTaskBoundsConstraintsUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void apply_clampInputBoundsThatAreTooLarge() {
        // Arrange.
        var mockWindowManager = mock(WindowManager.class);
        ChromeAndroidTaskUnitTestSupport.mockMaxWindowMetrics(
                mockWindowManager,
                DEFAULT_FULL_SCREEN_BOUNDS_IN_PX,
                DEFAULT_MAX_TAPPABLE_INSETS_IN_PX);

        Rect inputBoundsInPx = new Rect(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX);
        inputBoundsInPx.offset(/* dx= */ 0, /* dy= */ 10);

        // Act.
        Rect adjustedBounds =
                ChromeAndroidTaskBoundsConstraints.apply(inputBoundsInPx, mockWindowManager);

        // Assert.
        assertEquals(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, adjustedBounds);
    }

    @Test
    public void getMaxBoundsInPx() {
        // Arrange.
        var mockWindowManager = mock(WindowManager.class);
        ChromeAndroidTaskUnitTestSupport.mockMaxWindowMetrics(
                mockWindowManager,
                DEFAULT_FULL_SCREEN_BOUNDS_IN_PX,
                DEFAULT_MAX_TAPPABLE_INSETS_IN_PX);

        // Act.
        Rect maxBounds = ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(mockWindowManager);

        // Assert.
        assertEquals(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, maxBounds);
    }
}
