// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.displaystyle;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for @{@link ViewResizerUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class ViewResizerUtilTest {

    private Context mContext;

    @Before
    public void setup() {
        mContext = RuntimeEnvironment.getApplication().getApplicationContext();
    }

    @Test
    @Config(qualifiers = "w320dp-h1024dp")
    public void computePadding_narrowDisplay() {
        // computedPadding = (screen_width - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP)/2
        // = (320 - 600)/2 < 0.
        int expectedPadding = 20;
        int res =
                ViewResizerUtil.computePaddingForWideDisplay(
                        mContext, null, /* minWidePaddingPixels= */ expectedPadding);
        assertEquals("Padding is not as expected.", expectedPadding, res);
    }

    @Test
    @Config(qualifiers = "w700dp-h1024dp")
    public void computePadding_wideDisplay() {
        //  (screen_width - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP)/2 = (700 - 600)/2
        int expectedPadding = 50;
        int res =
                ViewResizerUtil.computePaddingForWideDisplay(
                        mContext, null, /* minWidePaddingPixels= */ 20);
        assertEquals("Padding is not as expected.", expectedPadding, res);
    }

    @Test
    @Config(qualifiers = "w700dp-h1024dp")
    public void computePadding_wideDisplayUseMinPadding() {
        // computedPadding = (screen_width - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP)/2
        // = (700 - 600)/2 = 50
        int expectedPadding = 60;
        int res =
                ViewResizerUtil.computePaddingForWideDisplay(
                        mContext, null, /* minWidePaddingPixels= */ expectedPadding);
        assertEquals("Padding is not as expected.", expectedPadding, res);
    }
}
