// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

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
        @ColorInt
        int themeColor =
                SurfaceColorUpdateUtils.getDefaultThemeColor(mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceContainerHigh(mContext), themeColor);

        @ColorInt
        int omniboxColor =
                SurfaceColorUpdateUtils.getOmniboxBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurface(mContext), omniboxColor);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE})
    public void testThemeAndOmniboxColors_flagDisabled() {
        @ColorInt
        int themeColor =
                SurfaceColorUpdateUtils.getDefaultThemeColor(mContext, /* isIncognito= */ false);
        assertEquals(
                ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false), themeColor);

        @ColorInt
        int omniboxColor =
                SurfaceColorUpdateUtils.getOmniboxBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.toolbar_text_box_bg_color), omniboxColor);
    }

    @Test
    public void testThemeAndOmniboxColors_Incognito() {
        @ColorInt
        int themeColor =
                SurfaceColorUpdateUtils.getDefaultThemeColor(mContext, /* isIncognito= */ true);
        assertEquals(
                ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ true), themeColor);

        @ColorInt
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
        @ColorInt
        int gtsBackgroundColor =
                SurfaceColorUpdateUtils.getGridTabSwitcherBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceContainerHigh(mContext), gtsBackgroundColor);

        @ColorInt
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
        @ColorInt
        int gtsBackgroundColor =
                SurfaceColorUpdateUtils.getGridTabSwitcherBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getDefaultBgColor(mContext), gtsBackgroundColor);

        @ColorInt
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
        @ColorInt
        int tabCardViewBackgroundColor =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, /* colorId= */ null);
        assertEquals(SemanticColorUtils.getColorSurfaceDim(mContext), tabCardViewBackgroundColor);

        @ColorInt
        int tabCardViewBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true, /* colorId= */ null);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_dim_dark),
                tabCardViewBackgroundColorIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testTabCardViewBackgroundColor_FlagDisabled() {
        @ColorInt
        int tabCardViewBackgroundColor =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, /* colorId= */ null);
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainerHighest(mContext),
                tabCardViewBackgroundColor);

        @ColorInt
        int tabCardViewBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true, /* colorId= */ null);
        assertEquals(
                ContextCompat.getColor(
                        mContext, R.color.gm3_baseline_surface_container_highest_dark),
                tabCardViewBackgroundColorIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testGtsTabSearchBoxBackgroundColor_FlagEnabled() {
        @ColorInt
        int searchBoxBgColor =
                SurfaceColorUpdateUtils.getGtsSearchBoxBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurface(mContext), searchBoxBgColor);

        @ColorInt
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
        @ColorInt
        int searchBoxBgColor =
                SurfaceColorUpdateUtils.getGtsSearchBoxBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceContainerHigh(mContext), searchBoxBgColor);

        @ColorInt
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
        @ColorInt
        int messageCardBackgroundColor =
                SurfaceColorUpdateUtils.getMessageCardBackgroundColor(mContext);
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainerLow(mContext),
                messageCardBackgroundColor);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testMessageCardBackgroundColor_FlagDisabled() {
        @ColorInt
        int messageCardBackgroundColor =
                SurfaceColorUpdateUtils.getMessageCardBackgroundColor(mContext);
        assertEquals(
                SemanticColorUtils.getCardBackgroundColor(mContext), messageCardBackgroundColor);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testTabGridDialogColors_FlagEnabled() {
        @ColorInt
        int gtsBackgroundColor =
                SurfaceColorUpdateUtils.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurfaceContainer(mContext), gtsBackgroundColor);

        @ColorInt
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
        @ColorInt
        int gtsBackgroundColor =
                SurfaceColorUpdateUtils.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(SemanticColorUtils.getColorSurface(mContext), gtsBackgroundColor);

        @ColorInt
        int gtsBackgroundColorIncognito =
                SurfaceColorUpdateUtils.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_container_dark),
                gtsBackgroundColorIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    @DisableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE})
    public void testGetCardViewBackgroundColor_NewGm3TabGroupColorsEnabled_WithColorId() {
        @TabGroupColorId int blueColorId = TabGroupColorId.BLUE;
        @ColorInt
        int expectedColorFalse =
                TabGroupColorPickerUtils.getTabGroupCardColor(
                        mContext, blueColorId, /* isIncognito= */ false);
        @ColorInt
        int actualColorFalse =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, blueColorId);
        assertEquals(
                "Color mismatch for non-incognito with GM3 group colors and colorId.",
                expectedColorFalse,
                actualColorFalse);

        @ColorInt
        int expectedColorTrue =
                TabGroupColorPickerUtils.getTabGroupCardColor(
                        mContext, blueColorId, /* isIncognito= */ true);
        @ColorInt
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
            testGetCardViewBackgroundColor_NewGm3TabGroupColorsEnabled_WithColorId_GtsSurfaceAlsoEnabled() {
        @TabGroupColorId int greenColorId = TabGroupColorId.GREEN;
        @ColorInt
        int expectedColor =
                TabGroupColorPickerUtils.getTabGroupCardColor(
                        mContext, greenColorId, /* isIncognito= */ false);
        @ColorInt
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

        @ColorInt int expectedNonIncognito = SemanticColorUtils.getColorSurfaceDim(mContext);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, redColorId);
        assertEquals(
                "Color mismatch for non-incognito with GtsSurfaceColor (colorId ignored).",
                expectedNonIncognito,
                actualNonIncognito);

        @ColorInt
        int expectedIncognito =
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_dim_dark);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ true, redColorId);
        assertEquals(
                "Color mismatch for incognito with GtsSurfaceColor (colorId ignored).",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewTextColor_NewGm3FlagEnabled_withColorId() {
        @TabGroupColorId int testColorId = TabGroupColorId.BLUE;

        // Test non-incognito.
        @ColorInt
        int expectedNonIncognito =
                TabGroupColorPickerUtils.getTabGroupCardTextColor(
                        mContext, testColorId, /* isIncognito= */ false);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewTextColor(
                        mContext, /* isIncognito= */ false, testColorId);
        assertEquals(
                "Text color mismatch for non-incognito with GM3 flag and colorId.",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito.
        @ColorInt
        int expectedIncognito =
                TabGroupColorPickerUtils.getTabGroupCardTextColor(
                        mContext, testColorId, /* isIncognito= */ true);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewTextColor(
                        mContext, /* isIncognito= */ true, testColorId);
        assertEquals(
                "Text color mismatch for incognito with GM3 flag and colorId.",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewTextColor_NewGm3FlagEnabled_colorIdNull() {

        // Test non-incognito with null colorId.
        @ColorInt int expectedNonIncognito = SemanticColorUtils.getDefaultTextColor(mContext);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewTextColor(
                        mContext, /* isIncognito= */ false, /* colorId= */ null);
        assertEquals(
                "Text color mismatch for non-incognito with GM3 flag and null colorId (fallback).",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito with null colorId.
        @ColorInt
        int expectedIncognito = ContextCompat.getColor(mContext, R.color.incognito_tab_title_color);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewTextColor(
                        mContext, /* isIncognito= */ true, /* colorId= */ null);
        assertEquals(
                "Text color mismatch for incognito with GM3 flag and null colorId (fallback).",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewTextColor_NewGm3FlagDisabled_withColorId() {

        @TabGroupColorId int testColorId = TabGroupColorId.RED;

        // Test non-incognito.
        @ColorInt int expectedNonIncognito = SemanticColorUtils.getDefaultTextColor(mContext);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewTextColor(
                        mContext, /* isIncognito= */ false, testColorId);
        assertEquals(
                "Text color mismatch for non-incognito with GM3 flag disabled (colorId ignored).",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito.
        @ColorInt
        int expectedIncognito = ContextCompat.getColor(mContext, R.color.incognito_tab_title_color);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewTextColor(
                        mContext, /* isIncognito= */ true, testColorId);
        assertEquals(
                "Text color mismatch for incognito with GM3 flag disabled (colorId ignored).",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewTextColor_NewGm3FlagDisabled_colorIdNull() {

        // Test non-incognito.
        @ColorInt int expectedNonIncognito = SemanticColorUtils.getDefaultTextColor(mContext);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewTextColor(
                        mContext, /* isIncognito= */ false, /* colorId= */ null);
        assertEquals(
                "Text color mismatch for non-incognito with GM3 flag disabled and null colorId.",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito.
        @ColorInt
        int expectedIncognito = ContextCompat.getColor(mContext, R.color.incognito_tab_title_color);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewTextColor(
                        mContext, /* isIncognito= */ true, /* colorId= */ null);
        assertEquals(
                "Text color mismatch for incognito with GM3 flag disabled and null colorId.",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewGroupNumberTextColor_gm3FlagEnabled_withColorId() {
        @TabGroupColorId int testColorId = TabGroupColorId.CYAN;

        // Test non-incognito.
        @ColorInt
        int expectedNonIncognito =
                TabGroupColorPickerUtils.getTabGroupCardTextColor(
                        mContext, testColorId, /* isIncognito= */ false);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewGroupNumberTextColor(
                        mContext, /* isIncognito= */ false, testColorId);
        assertEquals(
                "Group number text color mismatch for non-incognito with GM3 flag and colorId.",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito.
        @ColorInt
        int expectedIncognito =
                TabGroupColorPickerUtils.getTabGroupCardTextColor(
                        mContext, testColorId, /* isIncognito= */ true);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewGroupNumberTextColor(
                        mContext, /* isIncognito= */ true, testColorId);
        assertEquals(
                "Group number text color mismatch for incognito with GM3 flag and colorId.",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewGroupNumberTextColor_gm3FlagDisabled() {

        @TabGroupColorId int testColorId = TabGroupColorId.PINK;

        // Test non-incognito (fallback path).
        @ColorInt int expectedNonIncognito = SemanticColorUtils.getDefaultTextColor(mContext);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewGroupNumberTextColor(
                        mContext, /* isIncognito= */ false, testColorId);
        assertEquals(
                "Group number text color mismatch for non-incognito with GM3 flag disabled (colorId"
                        + " ignored).",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito (fallback path).
        @ColorInt
        int expectedIncognito =
                ContextCompat.getColor(mContext, R.color.incognito_tab_tile_number_color);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewGroupNumberTextColor(
                        mContext, /* isIncognito= */ true, testColorId);
        assertEquals(
                "Group number text color mismatch for incognito with GM3 flag disabled (colorId"
                        + " ignored).",
                expectedIncognito,
                actualIncognito);

        // Test with a null colorId to ensure it behaves the same as with a non-null colorId when
        // the flag is off.
        @ColorInt
        int actualNonIncognitoNullId =
                SurfaceColorUpdateUtils.getCardViewGroupNumberTextColor(
                        mContext, /* isIncognito= */ false, /* colorId= */ null);
        assertEquals(
                "Group number text color should be the same for null and non-null colorId when GM3"
                        + " flag is disabled.",
                expectedNonIncognito,
                actualNonIncognitoNullId);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewMiniThumbnailPlaceholderColor_gm3FlagEnabled_withColorId() {
        @TabGroupColorId int testColorId = TabGroupColorId.PURPLE;

        // Test non-incognito.
        @ColorInt
        int expectedNonIncognito =
                TabGroupColorPickerUtils.getTabGroupCardMiniThumbnailPlaceholderColor(
                        mContext, testColorId, /* isIncognito= */ false);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewMiniThumbnailPlaceholderColor(
                        mContext, /* isIncognito= */ false, testColorId);
        assertEquals(
                "Placeholder color mismatch for non-incognito with GM3 flag and colorId.",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito.
        @ColorInt
        int expectedIncognito =
                TabGroupColorPickerUtils.getTabGroupCardMiniThumbnailPlaceholderColor(
                        mContext, testColorId, /* isIncognito= */ true);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewMiniThumbnailPlaceholderColor(
                        mContext, /* isIncognito= */ true, testColorId);
        assertEquals(
                "Placeholder color mismatch for incognito with GM3 flag and colorId.",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewMiniThumbnailPlaceholderColor_gm3FlagDisabled() {

        @TabGroupColorId int testColorId = TabGroupColorId.ORANGE;

        // Test non-incognito (fallback path).
        @ColorInt
        int expectedNonIncognito = SemanticColorUtils.getColorSurfaceContainerLow(mContext);
        @ColorInt
        int actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewMiniThumbnailPlaceholderColor(
                        mContext, /* isIncognito= */ false, testColorId);
        assertEquals(
                "Placeholder color mismatch for non-incognito with GM3 flag disabled (colorId"
                        + " ignored).",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito (fallback path).
        @ColorInt
        int expectedIncognito =
                ContextCompat.getColor(mContext, R.color.incognito_tab_thumbnail_placeholder_color);
        @ColorInt
        int actualIncognito =
                SurfaceColorUpdateUtils.getCardViewMiniThumbnailPlaceholderColor(
                        mContext, /* isIncognito= */ true, testColorId);
        assertEquals(
                "Placeholder color mismatch for incognito with GM3 flag disabled (colorId"
                        + " ignored).",
                expectedIncognito,
                actualIncognito);

        // Test with a null colorId to ensure it behaves the same as with a non-null colorId when
        // the flag is off.
        @ColorInt
        int actualNonIncognitoNullId =
                SurfaceColorUpdateUtils.getCardViewMiniThumbnailPlaceholderColor(
                        mContext, /* isIncognito= */ false, /* colorId= */ null);
        assertEquals(
                "Placeholder color should be the same for null and non-null colorId when GM3 flag"
                        + " is disabled.",
                expectedNonIncognito,
                actualNonIncognitoNullId);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewActionButtonColor_gm3FlagEnabled_withColorId() {
        @TabGroupColorId int testColorId = TabGroupColorId.RED;

        // Test non-incognito.
        ColorStateList expectedNonIncognito =
                ColorStateList.valueOf(
                        TabGroupColorPickerUtils.getTabGroupCardTextColor(
                                mContext, testColorId, /* isIncognito= */ false));
        ColorStateList actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewActionButtonColor(
                        mContext, /* isIncognito= */ false, testColorId);
        assertEquals(
                "Action button color mismatch for non-incognito with GM3 flag and colorId.",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito.
        ColorStateList expectedIncognito =
                ColorStateList.valueOf(
                        TabGroupColorPickerUtils.getTabGroupCardTextColor(
                                mContext, testColorId, /* isIncognito= */ true));
        ColorStateList actualIncognito =
                SurfaceColorUpdateUtils.getCardViewActionButtonColor(
                        mContext, /* isIncognito= */ true, testColorId);
        assertEquals(
                "Action button color mismatch for incognito with GM3 flag and colorId.",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewActionButtonColor_gm3FlagEnabled_colorIdNull() {
        // When the GM3 flag is enabled but the colorId is null, the function should
        // execute its fallback logic.

        // Test non-incognito with a null colorId.
        ColorStateList expectedNonIncognito =
                ColorStateList.valueOf(
                        MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, ""));
        ColorStateList actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewActionButtonColor(
                        mContext, /* isIncognito= */ false, /* colorId= */ null);
        assertEquals(
                "Action button color should use fallback for non-incognito when colorId is null.",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito with a null colorId.
        ColorStateList expectedIncognito =
                AppCompatResources.getColorStateList(
                        mContext, R.color.incognito_tab_action_button_color);
        ColorStateList actualIncognito =
                SurfaceColorUpdateUtils.getCardViewActionButtonColor(
                        mContext, /* isIncognito= */ true, /* colorId= */ null);
        assertEquals(
                "Action button color should use fallback for incognito when colorId is null.",
                expectedIncognito,
                actualIncognito);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3})
    public void testGetCardViewActionButtonColor_gm3FlagDisabled() {
        // If the GM3 flag is disabled, the colorId should be ignored and the fallback logic used.
        @TabGroupColorId int testColorId = TabGroupColorId.GREEN;

        // Test non-incognito (fallback path).
        ColorStateList expectedNonIncognito =
                ColorStateList.valueOf(
                        MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, ""));
        ColorStateList actualNonIncognito =
                SurfaceColorUpdateUtils.getCardViewActionButtonColor(
                        mContext, /* isIncognito= */ false, testColorId);
        assertEquals(
                "Action button color mismatch for non-incognito with GM3 flag disabled (colorId"
                        + " ignored).",
                expectedNonIncognito,
                actualNonIncognito);

        // Test incognito (fallback path).
        ColorStateList expectedIncognito =
                AppCompatResources.getColorStateList(
                        mContext, R.color.incognito_tab_action_button_color);
        ColorStateList actualIncognito =
                SurfaceColorUpdateUtils.getCardViewActionButtonColor(
                        mContext, /* isIncognito= */ true, testColorId);
        assertEquals(
                "Action button color mismatch for incognito with GM3 flag disabled (colorId"
                        + " ignored).",
                expectedIncognito,
                actualIncognito);

        // Test with a null colorId to ensure it behaves the same as with a non-null colorId when
        // the flag is off.
        ColorStateList actualNonIncognitoNullId =
                SurfaceColorUpdateUtils.getCardViewActionButtonColor(
                        mContext, /* isIncognito= */ false, /* colorId= */ null);
        assertEquals(
                "Action button color should be the same for null and non-null colorId when GM3 flag"
                        + " is disabled.",
                expectedNonIncognito,
                actualNonIncognitoNullId);
    }
}
