// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.core.content.ContextCompat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

@RunWith(BaseRobolectricTestRunner.class)
public class SurfaceColorUpdateUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE})
    public void testThemeAndOmniboxColors_flagEnabled() {
        int themeColor =
                SurfaceColorUpdateUtils.getDefaultThemeColor(mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceContainerHigh(mContext), themeColor);

        int omniboxColor =
                SurfaceColorUpdateUtils.getOmniboxBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurface(mContext), omniboxColor);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE})
    public void testThemeAndOmniboxColors_flagDisabled() {
        int themeColor =
                SurfaceColorUpdateUtils.getDefaultThemeColor(mContext, /* isIncognito= */ false);
        assertEquals(
                ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false), themeColor);

        int omniboxColor =
                SurfaceColorUpdateUtils.getOmniboxBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.toolbar_text_box_bg_color), omniboxColor);
    }

    @Test
    public void testThemeAndOmniboxColors_Incognito() {
        int themeColor =
                SurfaceColorUpdateUtils.getDefaultThemeColor(mContext, /* isIncognito= */ true);
        assertEquals(
                ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ true), themeColor);

        int omniboxColor =
                SurfaceColorUpdateUtils.getOmniboxBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.toolbar_text_box_background_incognito),
                omniboxColor);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testGtsColors_FlagEnabled() {
        int gtsBackgroundColor =
                SurfaceColorUpdateUtils.getGridTabSwitcherBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceContainerHigh(mContext), gtsBackgroundColor);

        int gtsBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getGridTabSwitcherBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_container_high_dark),
                gtsBackgroundColorIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testGtsColors_FlagDisabled() {
        int gtsBackgroundColor =
                SurfaceColorUpdateUtils.getGridTabSwitcherBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getDefaultBgColor(mContext), gtsBackgroundColor);

        int gtsBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getGridTabSwitcherBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.default_bg_color_dark),
                gtsBackgroundColorIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testTabCardViewBackgroundColor_FlagEnabled() {
        int tabCardViewBackgroundColor =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, /* colorId */ null);
        assertEquals(SemanticColorUtils.getColorSurfaceDim(mContext), tabCardViewBackgroundColor);

        int tabCardViewBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true, /* colorId */ null);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_dim_dark),
                tabCardViewBackgroundColorIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testTabCardVieBackgroundColor_FlagDisabled() {
        int tabCardViewBackgroundColor =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, /* colorId */ null);
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainerHigh(mContext),
                tabCardViewBackgroundColor);

        int tabCardViewBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true, /* colorId */ null);
        assertEquals(
                ContextCompat.getColor(
                        mContext, R.color.gm3_baseline_surface_container_highest_dark),
                tabCardViewBackgroundColorIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testGtsTabSearchBoxBackgroundColor_FlagEnabled() {
        int searchBoxBgColor =
                SurfaceColorUpdateUtils.getGtsSearchBoxBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurface(mContext), searchBoxBgColor);

        int searchBoxBgColorIncognito =
                SurfaceColorUpdateUtils.getGtsSearchBoxBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_dark),
                searchBoxBgColorIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testGtsTabSearchBoxBackgroundColor_FlagDisabled() {
        int searchBoxBgColor =
                SurfaceColorUpdateUtils.getGtsSearchBoxBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceContainerHigh(mContext), searchBoxBgColor);

        int searchBoxBgColorIncognito =
                SurfaceColorUpdateUtils.getGtsSearchBoxBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(
                        mContext, R.color.gm3_baseline_surface_container_highest_dark),
                searchBoxBgColorIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testMessageCardBackgroundColor_FlagEnabled() {
        int messageCardBackgroundColor =
                SurfaceColorUpdateUtils.getMessageCardBackgroundColor(mContext);
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainerLow(mContext),
                messageCardBackgroundColor);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testMessageCardBackgroundColor_FlagDisabled() {
        int messageCardBackgroundColor =
                SurfaceColorUpdateUtils.getMessageCardBackgroundColor(mContext);
        assertEquals(
                SemanticColorUtils.getCardBackgroundColor(mContext), messageCardBackgroundColor);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testTabGridDialogColors_FlagEnabled() {
        int gtsBackgroundColor =
                SurfaceColorUpdateUtils.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceContainer(mContext), gtsBackgroundColor);

        int gtsBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_container_dark),
                gtsBackgroundColorIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testTabGridDialogColors_FlagDisabled() {
        int gtsBackgroundColor =
                SurfaceColorUpdateUtils.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurface(mContext), gtsBackgroundColor);

        int gtsBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_container_low_dark),
                gtsBackgroundColorIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testGetCardViewBackgroundColor_NewGM3TabGroupColorsEnabled_WithColorId() {
        @TabGroupColorId int blueColorId = TabGroupColorId.BLUE;
        int expectedColorFalse =
                TabGroupColorPickerUtils.getTabGroupCardColor(
                        mContext, /* isIncognito= */ false, blueColorId);
        int actualColorFalse =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, blueColorId);
        assertEquals(
                "Color mismatch for non-incognito with GM3 group colors and colorId.",
                expectedColorFalse,
                actualColorFalse);

        int expectedColorTrue =
                TabGroupColorPickerUtils.getTabGroupCardColor(
                        mContext, /* isIncognito= */ true, blueColorId);
        int actualColorTrue =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true, blueColorId);
        assertEquals(
                "Color mismatch for incognito with GM3 group colors and colorId.",
                expectedColorTrue,
                actualColorTrue);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3,
        ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE
    })
    public void
            testGetCardViewBackgroundColor_NewGM3TabGroupColorsEnabled_WithColorId_GtsSurfaceAlsoEnabled() {
        @TabGroupColorId int greenColorId = TabGroupColorId.GREEN;
        int expectedColor =
                TabGroupColorPickerUtils.getTabGroupCardColor(
                        mContext, /* isIncognito= */ false, greenColorId);
        int actualColor =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, greenColorId);
        assertEquals(
                "GM3 group colors should take precedence when colorId is present.",
                expectedColor,
                actualColor);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    @EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testGetCardViewBackgroundColor_NewGtsSurfaceColorEnabled_ColorIdNotNull() {
        @TabGroupColorId int redColorId = TabGroupColorId.RED;

        int expectedNonIncognito = SemanticColorUtils.getColorSurfaceDim(mContext);
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, redColorId);
        assertEquals(
                "Color mismatch for non-incognito with GtsSurfaceColor (colorId ignored).",
                expectedNonIncognito,
                actualNonIncognito);

        int expectedIncognito =
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_dim_dark);
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true, redColorId);
        assertEquals(
                "Color mismatch for incognito with GtsSurfaceColor (colorId ignored).",
                expectedIncognito,
                actualIncognito);
    }
}
