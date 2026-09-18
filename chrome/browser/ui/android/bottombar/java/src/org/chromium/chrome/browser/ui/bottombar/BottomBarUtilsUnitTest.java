// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;

import android.content.Context;

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
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BottomBarUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw300dp-mdpi")
public class BottomBarUtilsUnitTest {
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
}
