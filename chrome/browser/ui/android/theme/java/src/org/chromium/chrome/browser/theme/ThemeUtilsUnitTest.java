// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

@RunWith(BaseRobolectricTestRunner.class)
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
                        mContext, Color.BLACK, /* incognito= */ true, /* isCustomTab= */ false);
        assertEquals(mContext.getColor(R.color.toolbar_text_box_background_incognito), themeColor);

        themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.BLACK, /* incognito= */ true, /* isCustomTab= */ true);
        assertEquals(mContext.getColor(R.color.toolbar_text_box_background_incognito), themeColor);
    }

    @Test
    public void getTextBoxColorForToolbarBackgroundInNonNativePage_anyDark() {
        int themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.BLACK, /* incognito= */ false, /* isCustomTab= */ false);
        assertEquals(
                ColorUtils.getColorWithOverlay(
                        Color.BLACK,
                        Color.WHITE,
                        ThemeUtils.LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA),
                themeColor);

        themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.BLACK, /* incognito= */ false, /* isCustomTab= */ true);
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
                        mContext, 0xfffffe, /* incognito= */ false, /* isCustomTab= */ true);
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
                        mContext, 0xfffffe, /* incognito= */ false, /* isCustomTab= */ false);
        assertEquals(Color.WHITE, themeColor);
    }

    @Test
    public void getTextBoxColorForToolbarBackgroundInNonNativePage_anyDefault() {
        float tabElevation = mContext.getResources().getDimension(R.dimen.default_elevation_4);
        int expectedColor = ChromeColors.getSurfaceColor(mContext, tabElevation);

        int themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.WHITE, /* incognito= */ false, /* isCustomTab= */ false);
        assertEquals(expectedColor, themeColor);

        themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext, Color.WHITE, /* incognito= */ false, /* isCustomTab= */ true);
        assertEquals(expectedColor, themeColor);
    }
}
