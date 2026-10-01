// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.RippleDrawable;
import android.view.Gravity;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.android.bars_common.CaptureSafeRippleDrawable;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BottomBarUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw300dp-mdpi")
public class BottomBarUtilsUnitTest {
    private static class RecordingDrawable extends ColorDrawable {
        boolean mWasDrawn;

        @Override
        public void draw(Canvas canvas) {
            mWasDrawn = true;
            super.draw(canvas);
        }
    }

    private ActivityController<TestActivity> mActivityController;
    private Context mContext;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mContext = mActivityController.get();
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testDimensions_Default60dp() {
        // Baseline 60dp. The new tab background stays at its declared 40dp, so the vertical
        // padding is (60 - 40) / 2 = 10px.
        assertEquals(60, BottomBarUtils.getBottomBarHeight(mContext));
        assertEquals(10, BottomBarUtils.getButtonPaddingVertical(mContext));
    }

    @Test
    @Config(qualifiers = "xhdpi")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/48")
    public void testDimensions_HighDensityXhdpi() {
        // At 2.0x density the bar is 48dp * 2 = 96px and the background 40dp * 2 = 80px, so the
        // vertical padding is (96 - 80) / 2 = 8px.
        assertEquals(96, BottomBarUtils.getBottomBarHeight(mContext));
        assertEquals(8, BottomBarUtils.getButtonPaddingVertical(mContext));
    }

    @Test
    public void testCreateHoverableRipple() {
        int expectedSize =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_new_tab_background_size);

        for (@BrandedColorScheme
        int scheme : new int[] {BrandedColorScheme.APP_DEFAULT, BrandedColorScheme.INCOGNITO}) {
            RippleDrawable ripple = BottomBarUtils.createHoverableRipple(mContext, scheme);
            assertTrue(ripple instanceof CaptureSafeRippleDrawable);
            assertEquals(1, ripple.getNumberOfLayers());
            assertEquals(android.R.id.content, ripple.getId(0));
            assertNotNull(ripple.getDrawable(0));
            assertEquals(expectedSize, ripple.getLayerWidth(0));
            assertEquals(expectedSize, ripple.getLayerHeight(0));
            assertEquals(Gravity.CENTER, ripple.getLayerGravity(0));
        }
    }

    @Test
    public void testCaptureSafeRippleDrawable_Draw_SkipsSoftwareCanvas() {
        RecordingDrawable content = new RecordingDrawable();
        Bitmap bitmap = Bitmap.createBitmap(40, 40, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        assertFalse(canvas.isHardwareAccelerated());

        CaptureSafeRippleDrawable drawable =
                new CaptureSafeRippleDrawable(
                        ColorStateList.valueOf(Color.RED), content, /* size= */ 40);
        drawable.draw(canvas);

        assertFalse(content.mWasDrawn);
    }

    @Test
    public void testCaptureSafeRippleDrawable_Draw_DrawsOnHardwareCanvas() {
        RecordingDrawable content = new RecordingDrawable();
        Bitmap bitmap = Bitmap.createBitmap(40, 40, Bitmap.Config.ARGB_8888);
        Canvas canvas =
                new Canvas(bitmap) {
                    @Override
                    public boolean isHardwareAccelerated() {
                        return true;
                    }
                };

        CaptureSafeRippleDrawable drawable =
                new CaptureSafeRippleDrawable(
                        ColorStateList.valueOf(Color.RED), content, /* size= */ 40);
        drawable.draw(canvas);

        assertTrue(content.mWasDrawn);
    }
}
