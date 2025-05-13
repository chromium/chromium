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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

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
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE})
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
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE})
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
    @Features.EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
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
    @Features.DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
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
    @Features.EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testTabCardViewBackgroundColor_FlagEnabled() {
        int tabCardViewBackgroundColor =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceDim(mContext), tabCardViewBackgroundColor);

        int tabCardViewBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_dim_dark),
                tabCardViewBackgroundColorIncognito);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testTabCardVieBackgroundColor_FlagDisabled() {
        int tabCardViewBackgroundColor =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainerHigh(mContext),
                tabCardViewBackgroundColor);

        int tabCardViewBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(
                        mContext, R.color.gm3_baseline_surface_container_highest_dark),
                tabCardViewBackgroundColorIncognito);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
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
    @Features.DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
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
    @Features.EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testMessageCardBackgroundColor_FlagEnabled() {
        int messageCardBackgroundColor =
                SurfaceColorUpdateUtils.getMessageCardBackgroundColor(mContext);
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainerLow(mContext),
                messageCardBackgroundColor);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testMessageCardBackgroundColor_FlagDisabled() {
        int messageCardBackgroundColor =
                SurfaceColorUpdateUtils.getMessageCardBackgroundColor(mContext);
        assertEquals(
                SemanticColorUtils.getCardBackgroundColor(mContext), messageCardBackgroundColor);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
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
    @Features.DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
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
}
