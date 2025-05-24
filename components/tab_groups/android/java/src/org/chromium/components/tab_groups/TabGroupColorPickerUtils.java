// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_groups;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Helper class to handle tab group color picker related utilities. */
@NullMarked
public class TabGroupColorPickerUtils {
    /**
     * Get the color corresponding to the color id that is passed in. Adjust the color depending on
     * light/dark/incognito mode as well as dynamic color themes. This function should only be used
     * for retrieving items from the tab group color picker.
     *
     * @param context The current context.
     * @param colorId The color id corresponding to the color item in the color picker.
     * @param isIncognito Whether the current tab model is in incognito mode.
     */
    public static @ColorInt int getTabGroupColorPickerItemColor(
            Context context, @TabGroupColorId int colorId, boolean isIncognito) {
        @ColorRes int colorRes = getTabGroupColorPickerItemColorResource(colorId, isIncognito);
        @ColorInt int color = ContextCompat.getColor(context, colorRes);

        if (isIncognito) {
            return color;
        } else {
            // Harmonize the resultant color with dynamic color themes if applicable. This will
            // no-op and return the passed in color if dynamic colors are not enabled.
            return MaterialColors.harmonizeWithPrimary(context, color);
        }
    }

    /**
     * Get the color resource corresponding to the respective color item. This function should only
     * be used for retrieving items from the tab group color picker.
     *
     * @param colorId The color id corresponding to the color item in the color picker.
     * @param isIncognito Whether the current tab model is in incognito mode.
     */
    public static @ColorRes int getTabGroupColorPickerItemColorResource(
            @TabGroupColorId int colorId, boolean isIncognito) {
        switch (colorId) {
            case TabGroupColorId.GREY:
                return isIncognito
                        ? R.color.tab_group_color_picker_grey_incognito
                        : R.color.tab_group_color_picker_grey;
            case TabGroupColorId.BLUE:
                return isIncognito
                        ? R.color.tab_group_color_picker_blue_incognito
                        : R.color.tab_group_color_picker_blue;
            case TabGroupColorId.RED:
                return isIncognito
                        ? R.color.tab_group_color_picker_red_incognito
                        : R.color.tab_group_color_picker_red;
            case TabGroupColorId.YELLOW:
                return isIncognito
                        ? R.color.tab_group_color_picker_yellow_incognito
                        : R.color.tab_group_color_picker_yellow;
            case TabGroupColorId.GREEN:
                return isIncognito
                        ? R.color.tab_group_color_picker_green_incognito
                        : R.color.tab_group_color_picker_green;
            case TabGroupColorId.PINK:
                return isIncognito
                        ? R.color.tab_group_color_picker_pink_incognito
                        : R.color.tab_group_color_picker_pink;
            case TabGroupColorId.PURPLE:
                return isIncognito
                        ? R.color.tab_group_color_picker_purple_incognito
                        : R.color.tab_group_color_picker_purple;
            case TabGroupColorId.CYAN:
                return isIncognito
                        ? R.color.tab_group_color_picker_cyan_incognito
                        : R.color.tab_group_color_picker_cyan;
            case TabGroupColorId.ORANGE:
                return isIncognito
                        ? R.color.tab_group_color_picker_orange_incognito
                        : R.color.tab_group_color_picker_orange;
            default:
                assert false : "Invalid tab group color id " + colorId;
                return Resources.ID_NULL;
        }
    }

    /**
     * Get the text color corresponding to the respective color item.
     *
     * @param context The current context.
     * @param colorId The color id corresponding to the color item in the color picker.
     * @param isIncognito Whether the current tab model is in incognito mode.
     */
    public static @ColorInt int getTabGroupColorPickerItemTextColor(
            Context context, @TabGroupColorId int colorId, boolean isIncognito) {
        if (isIncognito) {
            return ContextCompat.getColor(
                    context, R.color.tab_group_tab_strip_title_text_color_incognito);
        } else if (ColorUtils.inNightMode(context)) {
            return SemanticColorUtils.getColorOnSurfaceInverse(context);
        } else {
            if (shouldUseDarkTextColorOnTabGroupColor(colorId)) {
                return SemanticColorUtils.getDefaultTextColor(context);
            }
            return SemanticColorUtils.getDefaultTextColorOnAccent1(context);
        }
    }

    /**
     * @param colorId The color id corresponding to the color item in the color picker.
     * @return Whether dark text should be used.
     */
    public static boolean shouldUseDarkTextColorOnTabGroupColor(@TabGroupColorId int colorId) {
        switch (colorId) {
            case TabGroupColorId.YELLOW:
            case TabGroupColorId.ORANGE:
                return true;
            default:
                return false;
        }
    }
}
