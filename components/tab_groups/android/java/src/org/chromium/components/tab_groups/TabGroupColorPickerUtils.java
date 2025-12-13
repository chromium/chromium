// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_groups;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.StringRes;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.ValueUtils;

/** Helper class to handle tab group color picker related utilities. */
@NullMarked
public class TabGroupColorPickerUtils {

    private static final int[][] HOVERED_FOCUSED_PRESSED_AND_NORMAL_STATES =
            new int[][] {
                new int[] {android.R.attr.state_hovered},
                new int[] {android.R.attr.state_focused},
                new int[] {android.R.attr.state_pressed},
                new int[] {}
            };

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
        return resolveGroupRelatedColor(context, colorRes, isIncognito);
    }

    /**
     * Builds a {@link ColorStateList} for the ripple effect in the tab group color picker. The
     * ripple color adapts based on whether the current context is incognito or not, and applies
     * different alpha values for hovered, focused, and pressed states.
     *
     * @param context The {@link Context} used to retrieve colors and dimensions.
     * @param isIncognito A boolean indicating whether the current mode is incognito.
     * @return A {@link ColorStateList} configured for the ripple effect.
     */
    public static ColorStateList buildTabGroupColorPickerRippleColorStateList(
            Context context, boolean isIncognito) {
        @DimenRes int hoveredAlpha = R.dimen.tab_group_color_picker_hovered_alpha;
        @DimenRes int focusedAlpha = R.dimen.tab_group_color_picker_focused_alpha;
        @DimenRes int pressedAlpha = R.dimen.tab_group_color_picker_pressed_alpha;
        @ColorInt
        int onSurfaceColor =
                isIncognito
                        ? ContextCompat.getColor(
                                context, R.color.tab_group_color_picker_incognito_ripple_color)
                        : SemanticColorUtils.getColorOnSurface(context);

        int hoveredColor = getColorWithAlphaApplied(context, onSurfaceColor, hoveredAlpha);
        int focusedColor = getColorWithAlphaApplied(context, onSurfaceColor, focusedAlpha);
        int pressedColor = getColorWithAlphaApplied(context, onSurfaceColor, pressedAlpha);

        int[] colors = new int[] {hoveredColor, focusedColor, pressedColor, Color.TRANSPARENT};
        return new ColorStateList(HOVERED_FOCUSED_PRESSED_AND_NORMAL_STATES, colors);
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
        return switch (colorId) {
            case TabGroupColorId.GREY -> isIncognito
                    ? R.color.tab_group_color_picker_grey_incognito
                    : R.color.tab_group_color_picker_grey;
            case TabGroupColorId.BLUE -> isIncognito
                    ? R.color.tab_group_color_picker_blue_incognito
                    : R.color.tab_group_color_picker_blue;
            case TabGroupColorId.RED -> isIncognito
                    ? R.color.tab_group_color_picker_red_incognito
                    : R.color.tab_group_color_picker_red;
            case TabGroupColorId.YELLOW -> isIncognito
                    ? R.color.tab_group_color_picker_yellow_incognito
                    : R.color.tab_group_color_picker_yellow;
            case TabGroupColorId.GREEN -> isIncognito
                    ? R.color.tab_group_color_picker_green_incognito
                    : R.color.tab_group_color_picker_green;
            case TabGroupColorId.PINK -> isIncognito
                    ? R.color.tab_group_color_picker_pink_incognito
                    : R.color.tab_group_color_picker_pink;
            case TabGroupColorId.PURPLE -> isIncognito
                    ? R.color.tab_group_color_picker_purple_incognito
                    : R.color.tab_group_color_picker_purple;
            case TabGroupColorId.CYAN -> isIncognito
                    ? R.color.tab_group_color_picker_cyan_incognito
                    : R.color.tab_group_color_picker_cyan;
            case TabGroupColorId.ORANGE -> isIncognito
                    ? R.color.tab_group_color_picker_orange_incognito
                    : R.color.tab_group_color_picker_orange;
            default -> {
                assert false : "Invalid tab group color id " + colorId;
                yield Resources.ID_NULL;
            }
        };
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
        return switch (colorId) {
            case TabGroupColorId.YELLOW, TabGroupColorId.ORANGE -> true;
            default -> false;
        };
    }

    /**
     * Get the color corresponding to the color id that is passed in. Adjust the color depending on
     * light/dark/incognito mode as well as dynamic color themes. This function should only be used
     * for retrieving items from the tab group card color.
     *
     * @param context The current context.
     * @param colorId The color id corresponding to the color of the Tab Group.
     * @param isIncognito Whether the current tab model is in incognito mode.
     */
    public static @ColorInt int getTabGroupCardColor(
            Context context, @TabGroupColorId int colorId, boolean isIncognito) {
        @ColorRes int colorRes = getTabGroupCardColorResource(colorId, isIncognito);
        return resolveGroupRelatedColor(context, colorRes, isIncognito);
    }

    /**
     * Get the color resource corresponding to the respective color item. This function should only
     * be used for retrieving items from the tab group card color.
     *
     * @param colorId The color id corresponding to the color of the Tab Group.
     */
    public static @ColorRes int getTabGroupCardColorResource(
            @TabGroupColorId int colorId, boolean isIncognito) {
        return switch (colorId) {
            case TabGroupColorId.GREY -> isIncognito
                    ? R.color.tab_group_card_color_grey_incognito
                    : R.color.tab_group_card_color_grey;
            case TabGroupColorId.BLUE -> isIncognito
                    ? R.color.tab_group_card_color_blue_incognito
                    : R.color.tab_group_card_color_blue;
            case TabGroupColorId.RED -> isIncognito
                    ? R.color.tab_group_card_color_red_incognito
                    : R.color.tab_group_card_color_red;
            case TabGroupColorId.YELLOW -> isIncognito
                    ? R.color.tab_group_card_color_yellow_incognito
                    : R.color.tab_group_card_color_yellow;
            case TabGroupColorId.GREEN -> isIncognito
                    ? R.color.tab_group_card_color_green_incognito
                    : R.color.tab_group_card_color_green;
            case TabGroupColorId.PINK -> isIncognito
                    ? R.color.tab_group_card_color_pink_incognito
                    : R.color.tab_group_card_color_pink;
            case TabGroupColorId.PURPLE -> isIncognito
                    ? R.color.tab_group_card_color_purple_incognito
                    : R.color.tab_group_card_color_purple;
            case TabGroupColorId.CYAN -> isIncognito
                    ? R.color.tab_group_card_color_cyan_incognito
                    : R.color.tab_group_card_color_cyan;
            case TabGroupColorId.ORANGE -> isIncognito
                    ? R.color.tab_group_card_color_orange_incognito
                    : R.color.tab_group_card_color_orange;
            default -> {
                assert false : "Invalid tab group color id " + colorId;
                yield Resources.ID_NULL;
            }
        };
    }

    /**
     * Get the color corresponding to the color id that is passed in. Adjust the color depending on
     * light/dark/incognito mode as well as dynamic color themes. This function should only be used
     * for retrieving items from the tab group card text color.
     *
     * @param context The current context.
     * @param colorId The color id corresponding to the color of the Tab Group.
     * @param isIncognito Whether the current tab model is in incognito mode.
     */
    public static @ColorInt int getTabGroupCardTextColor(
            Context context, @TabGroupColorId int colorId, boolean isIncognito) {
        @ColorRes int colorRes = getTabGroupCardTextColorResource(colorId, isIncognito);
        return resolveGroupRelatedColor(context, colorRes, isIncognito);
    }

    /**
     * Get the color resource corresponding to the respective Tab Group Text color. This function
     * should only be used for retrieving items from the tab group text color.
     *
     * @param colorId The color id corresponding to the color of the Tab Group.
     */
    public static @ColorRes int getTabGroupCardTextColorResource(
            @TabGroupColorId int colorId, boolean isIncognito) {
        return switch (colorId) {
            case TabGroupColorId.GREY -> isIncognito
                    ? R.color.tab_group_card_text_color_grey_incognito
                    : R.color.tab_group_card_text_color_grey;
            case TabGroupColorId.BLUE -> isIncognito
                    ? R.color.tab_group_card_text_color_blue_incognito
                    : R.color.tab_group_card_text_color_blue;
            case TabGroupColorId.RED -> isIncognito
                    ? R.color.tab_group_card_text_color_red_incognito
                    : R.color.tab_group_card_text_color_red;
            case TabGroupColorId.YELLOW -> isIncognito
                    ? R.color.tab_group_card_text_color_yellow_incognito
                    : R.color.tab_group_card_text_color_yellow;
            case TabGroupColorId.GREEN -> isIncognito
                    ? R.color.tab_group_card_text_color_green_incognito
                    : R.color.tab_group_card_text_color_green;
            case TabGroupColorId.PINK -> isIncognito
                    ? R.color.tab_group_card_text_color_pink_incognito
                    : R.color.tab_group_card_text_color_pink;
            case TabGroupColorId.PURPLE -> isIncognito
                    ? R.color.tab_group_card_text_color_purple_incognito
                    : R.color.tab_group_card_text_color_purple;
            case TabGroupColorId.CYAN -> isIncognito
                    ? R.color.tab_group_card_text_color_cyan_incognito
                    : R.color.tab_group_card_text_color_cyan;
            case TabGroupColorId.ORANGE -> isIncognito
                    ? R.color.tab_group_card_text_color_orange_incognito
                    : R.color.tab_group_card_text_color_orange;
            default -> {
                assert false : "Invalid tab group text color id " + colorId;
                yield Resources.ID_NULL;
            }
        };
    }

    /**
     * Get the color corresponding to the color id that is passed in. Adjust the color depending on
     * light/dark/incognito mode as well as dynamic color themes. This function should only be used
     * for retrieving items from the tab group card mini thumbnail placeholder color.
     *
     * @param context The current context.
     * @param colorId The color id corresponding to the color item in the color picker.
     * @param isIncognito Whether the current tab model is in incognito mode.
     */
    public static @ColorInt int getTabGroupCardMiniThumbnailPlaceholderColor(
            Context context, @TabGroupColorId int colorId, boolean isIncognito) {
        @ColorRes
        int colorRes = getTabGroupCardMiniThumbnailPlaceholderColorResource(colorId, isIncognito);
        return resolveGroupRelatedColor(context, colorRes, isIncognito);
    }

    /**
     * Get the color resource corresponding to the respective color item. This function should only
     * be used for retrieving items from the tab group color.
     *
     * @param colorId The color id corresponding to the color of the Tab Group.
     */
    public static @ColorRes int getTabGroupCardMiniThumbnailPlaceholderColorResource(
            @TabGroupColorId int colorId, boolean isIncognito) {
        return switch (colorId) {
            case TabGroupColorId.GREY -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_grey_incognito
                    : R.color.tab_group_card_placeholder_color_grey;
            case TabGroupColorId.BLUE -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_blue_incognito
                    : R.color.tab_group_card_placeholder_color_blue;
            case TabGroupColorId.RED -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_red_incognito
                    : R.color.tab_group_card_placeholder_color_red;
            case TabGroupColorId.YELLOW -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_yellow_incognito
                    : R.color.tab_group_card_placeholder_color_yellow;
            case TabGroupColorId.GREEN -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_green_incognito
                    : R.color.tab_group_card_placeholder_color_green;
            case TabGroupColorId.PINK -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_pink_incognito
                    : R.color.tab_group_card_placeholder_color_pink;
            case TabGroupColorId.PURPLE -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_purple_incognito
                    : R.color.tab_group_card_placeholder_color_purple;
            case TabGroupColorId.CYAN -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_cyan_incognito
                    : R.color.tab_group_card_placeholder_color_cyan;
            case TabGroupColorId.ORANGE -> isIncognito
                    ? R.color.tab_group_card_placeholder_color_orange_incognito
                    : R.color.tab_group_card_placeholder_color_orange;
            default -> {
                assert false : "Invalid tab group text color id " + colorId;
                yield Resources.ID_NULL;
            }
        };
    }

    /**
     * Get the {@link TabGroupColorId} associated with a tab group color plain integer. This
     * function should only be used for mapping a tab group color back to its IntDef value.
     *
     * @param colorId The plain color id corresponding to the color of the Tab Group.
     */
    public static @TabGroupColorId int getTabGroupCardColorId(int colorId) {
        return switch (colorId) {
                // LINT.IfChange
            case 0 -> TabGroupColorId.GREY;
            case 1 -> TabGroupColorId.BLUE;
            case 2 -> TabGroupColorId.RED;
            case 3 -> TabGroupColorId.YELLOW;
            case 4 -> TabGroupColorId.GREEN;
            case 5 -> TabGroupColorId.PINK;
            case 6 -> TabGroupColorId.PURPLE;
            case 7 -> TabGroupColorId.CYAN;
            case 8 -> TabGroupColorId.ORANGE;
            default -> {
                assert false : "Invalid tab group color id " + colorId;
                yield TabGroupColorId.GREY;
            }
                // LINT.ThenChange(//components/tab_groups/tab_group_color.h)
        };
    }

    /**
     * Get the accessibility string corresponding to the respective color item. This function should
     * only be used for retrieving items from the tab group color picker.
     *
     * @param colorId The color id corresponding to the color item in the color picker.
     */
    public static @StringRes int getTabGroupColorPickerItemColorAccessibilityString(
            @TabGroupColorId int colorId) {
        switch (colorId) {
            case TabGroupColorId.GREY:
                return R.string.tab_group_color_grey;
            case TabGroupColorId.BLUE:
                return R.string.tab_group_color_blue;
            case TabGroupColorId.RED:
                return R.string.tab_group_color_red;
            case TabGroupColorId.YELLOW:
                return R.string.tab_group_color_yellow;
            case TabGroupColorId.GREEN:
                return R.string.tab_group_color_green;
            case TabGroupColorId.PINK:
                return R.string.tab_group_color_pink;
            case TabGroupColorId.PURPLE:
                return R.string.tab_group_color_purple;
            case TabGroupColorId.CYAN:
                return R.string.tab_group_color_cyan;
            case TabGroupColorId.ORANGE:
                return R.string.tab_group_color_orange;
            default:
                assert false : "Invalid tab group color id " + colorId;
                return Resources.ID_NULL;
        }
    }

    private static @ColorInt int resolveGroupRelatedColor(
            Context context, @ColorRes int colorRes, boolean isIncognito) {
        @ColorInt int color = ContextCompat.getColor(context, colorRes);
        if (isIncognito) {
            return color;
        } else {
            // Harmonize the resultant color with dynamic color themes if applicable. This will
            // no-op and return the passed in color if dynamic colors are not enabled.
            return MaterialColors.harmonizeWithPrimary(context, color);
        }
    }

    private static @ColorInt int getColorWithAlphaApplied(
            Context context, int color, @DimenRes int alphaRes) {
        Resources resources = context.getResources();
        float alpha = ValueUtils.getFloat(resources, alphaRes);
        int alphaScaled = Math.round(alpha * 255);

        return ColorUtils.setAlphaComponent(color, alphaScaled);
    }
}
