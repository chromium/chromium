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
