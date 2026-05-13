// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.WindowManager;
import android.view.WindowMetrics;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link LensOverlayImageHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LensOverlayImageHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DisplayAndroid mDisplayAndroid;
    @Mock private Context mContext;
    @Mock private WindowManager mWindowManager;

    private static final int SCREEN_WIDTH = 2000;
    private static final int SCREEN_HEIGHT = 1000;
    private static final Rect WINDOW_BOUNDS = new Rect(100, 0, 1100, 1000);

    @Before
    public void setUp() {
        when(mWindowAndroid.getDisplay()).thenReturn(mDisplayAndroid);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mContext));
        when(mContext.getSystemService(Context.WINDOW_SERVICE)).thenReturn(mWindowManager);

        when(mDisplayAndroid.getDisplayWidth()).thenReturn(SCREEN_WIDTH);
        when(mDisplayAndroid.getDisplayHeight()).thenReturn(SCREEN_HEIGHT);
    }

    @Test
    @Config(sdk = android.os.Build.VERSION_CODES.R)
    public void getScreenMetrics_Success_Api30Plus() {
        // Mock API 30+ behavior.
        WindowMetrics mockMetrics = mock(WindowMetrics.class);
        when(mWindowManager.getCurrentWindowMetrics()).thenReturn(mockMetrics);
        when(mockMetrics.getBounds()).thenReturn(WINDOW_BOUNDS);

        LensOverlayImageHelper.LensOverlayScreenMetrics metrics =
                LensOverlayImageHelper.getScreenMetrics(mWindowAndroid);

        assertNotNull(metrics);
        assertEquals(SCREEN_WIDTH, metrics.screenWidth);
        assertEquals(SCREEN_HEIGHT, metrics.screenHeight);
        assertEquals(WINDOW_BOUNDS, metrics.windowBounds);
    }

    @Test
    @Config(sdk = android.os.Build.VERSION_CODES.Q)
    public void getScreenMetrics_Success_PreApi30() {
        // Before API 30, it should fall back to assuming full-screen bounds.
        LensOverlayImageHelper.LensOverlayScreenMetrics metrics =
                LensOverlayImageHelper.getScreenMetrics(mWindowAndroid);

        assertNotNull(metrics);
        assertEquals(SCREEN_WIDTH, metrics.screenWidth);
        assertEquals(SCREEN_HEIGHT, metrics.screenHeight);
        // Expecting a Rect from (0, 0) to (SCREEN_WIDTH, SCREEN_HEIGHT)
        assertEquals(new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), metrics.windowBounds);
    }

    @Test
    public void getScreenMetrics_NullContextReturnsNull() {
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));

        assertNull(LensOverlayImageHelper.getScreenMetrics(mWindowAndroid));
    }

    @Test
    public void compositeBitmap_Success() {
        LensOverlayImageHelper.LensOverlayScreenMetrics metrics =
                new LensOverlayImageHelper.LensOverlayScreenMetrics(
                        SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_BOUNDS);
        Bitmap windowshot = Bitmap.createBitmap(500, 500, Bitmap.Config.ARGB_8888);

        Bitmap result = LensOverlayImageHelper.compositeBitmap(metrics, windowshot);

        assertNotNull(result);
        assertEquals(SCREEN_WIDTH, result.getWidth());
        assertEquals(SCREEN_HEIGHT, result.getHeight());
        // Original windowshot should be recycled.
        assertTrue(windowshot.isRecycled());
    }

    @Test
    public void compositeBitmap_OomReturnsOriginal() {
        LensOverlayImageHelper.LensOverlayScreenMetrics metrics =
                new LensOverlayImageHelper.LensOverlayScreenMetrics(
                        Integer.MAX_VALUE, Integer.MAX_VALUE, WINDOW_BOUNDS);
        Bitmap windowshot = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        // This should trigger an OutOfMemoryError due to absurd dimensions.
        Bitmap result = LensOverlayImageHelper.compositeBitmap(metrics, windowshot);

        // Should fall back to the original windowshot.
        assertSame(windowshot, result);
    }
}
