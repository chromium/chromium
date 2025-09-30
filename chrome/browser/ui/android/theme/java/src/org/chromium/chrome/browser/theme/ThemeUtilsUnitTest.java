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
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
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
                        mContext,
                        ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false),
                        /* isIncognito= */ false,
                        /* isCustomTab= */ false);
        assertEquals(expectedColor, themeColor);

        themeColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mContext,
                        ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false),
                        /* isIncognito= */ false,
                        /* isCustomTab= */ true);
        assertEquals(expectedColor, themeColor);
    }

    @Test
    public void getThemedToolbarIconTintResForActivityState_anyBrandedThemeWithFocusActivity() {
        // DARK_BRANDED_THEME.
        int tintRes =
                ThemeUtils.getThemedToolbarIconTintResForActivityState(
                        BrandedColorScheme.DARK_BRANDED_THEME, /* isActivityFocused= */ false);
        assertEquals(R.color.toolbar_icon_unfocused_activity_light_color, tintRes);

        tintRes =
                ThemeUtils.getThemedToolbarIconTintResForActivityState(
                        BrandedColorScheme.DARK_BRANDED_THEME, /* isActivityFocused= */ true);
        assertEquals(R.color.default_icon_color_white_tint_list, tintRes);

        // LIGHT_BRANDED_THEME.
        tintRes =
                ThemeUtils.getThemedToolbarIconTintResForActivityState(
                        BrandedColorScheme.LIGHT_BRANDED_THEME, /* isActivityFocused= */ false);
        assertEquals(R.color.toolbar_icon_unfocused_activity_dark_color, tintRes);

        tintRes =
                ThemeUtils.getThemedToolbarIconTintResForActivityState(
                        BrandedColorScheme.LIGHT_BRANDED_THEME, /* isActivityFocused= */ true);
        assertEquals(R.color.default_icon_color_dark_tint_list, tintRes);
    }
}
