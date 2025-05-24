// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.core.content.ContextCompat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.util.ColorUtils;

@RunWith(BaseRobolectricTestRunner.class)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE
})
public class ThemeUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void getTextBoxColorForToolbarBackgroundInNonNativePage_anyIncognito() {
        int themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.BLACK, /* isIncognito= */ true, /* isCustomTab= */ false);
        assertEquals(mContext.getColor(R.color.toolbar_text_box_background_incognito), themeColor);

        themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.BLACK, /* isIncognito= */ true, /* isCustomTab= */ true);
        assertEquals(mContext.getColor(R.color.toolbar_text_box_background_incognito), themeColor);
    }

    @Test
    public void getTextBoxColorForToolbarBackgroundInNonNativePage_anyDark() {
        int themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.BLACK, /* isIncognito= */ false, /* isCustomTab= */ false);
        assertEquals(
                ColorUtils.getColorWithOverlay(
                        Color.BLACK,
                        Color.WHITE,
                        ThemeUtils.LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA),
                themeColor);

        themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.BLACK, /* isIncognito= */ false, /* isCustomTab= */ true);
        assertEquals(
                ColorUtils.getColorWithOverlay(
                        Color.BLACK,
                        Color.WHITE,
                        ThemeUtils.LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA),
                themeColor);
    }

    @Test
    public void getTextBoxColorForToolbarBackgroundInNonNativePage_customTabBright() {
        int themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, 0xfffffe, /* isIncognito= */ false, /* isCustomTab= */ true);
        assertEquals(
                ColorUtils.getColorWithOverlay(
                        0xfffffe,
                        Color.BLACK,
                        ThemeUtils.LOCATION_BAR_TRANSPARENT_BACKGROUND_DARKEN_ALPHA),
                themeColor);
    }

    @Test
    public void getTextBoxColorForToolbarBackgroundInNonNativePage_browserBright() {
        int themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, 0xfffffe, /* isIncognito= */ false, /* isCustomTab= */ false);
        assertEquals(Color.WHITE, themeColor);
    }

    @Test
    public void getTextBoxColorForToolbarBackgroundInNonNativePage_anyDefault() {
        int expectedColor = ContextCompat.getColor(mContext, R.color.toolbar_text_box_bg_color);

        int themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.WHITE, /* isIncognito= */ false, /* isCustomTab= */ false);
        assertEquals(expectedColor, themeColor);

        themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.WHITE, /* isIncognito= */ false, /* isCustomTab= */ true);
        assertEquals(expectedColor, themeColor);
    }
}
