// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;

/** Provides common default colors for Chrome UI. */
@NullMarked
public class ChromeColors {
    private static final String TAG = "ChromeColors";

    /**
     * Determines the default theme color used for toolbar based on the provided parameters.
     *
     * @param context {@link Context} used to retrieve colors.
     * @param isIncognito Whether the color is used in incognito mode. If true, this method will
     *     return a non-dynamic dark theme color.
     * @return The default theme color.
     */
    public static @ColorInt int getDefaultThemeColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.toolbar_background_incognito)
                : MaterialColors.getColor(context, R.attr.colorSurface, TAG);
    }

    /**
     * Returns the primary background color used as native page background based on the given
     * parameters.
     *
     * @param context The {@link Context} used to retrieve colors.
     * @param isIncognito Whether the color is used in incognito mode. If true, this method will
     *                    return a non-dynamic dark background color.
     * @return The primary background color.
     */
    public static @ColorInt int getPrimaryBackgroundColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.default_bg_color_dark)
                : SemanticColorUtils.getDefaultBgColor(context);
    }

    /**
     * Returns the large text primary style based on the given parameter.
     *
     * @param forceLightTextColor When true, returns the light-mode large text primary style;
     *     otherwise returns adaptive large text primary style.
     * @return The large text primary style.
     */
    public static int getLargeTextPrimaryStyle(boolean forceLightTextColor) {
        return forceLightTextColor
                ? R.style.TextAppearance_TextLarge_Primary_Baseline_Light
                : R.style.TextAppearance_TextLarge_Primary;
    }

    /**
     * Returns the text medium thick secondary style based on the incognito state.
     *
     * @param isIncognito When true, returns the baseline light medium text secondary style;
     *         otherwise returns adaptive medium text secondary style that can have dynamic colors.
     * @return The medium text secondary style.
     */
    public static int getTextMediumThickSecondaryStyle(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMediumThick_Secondary_Baseline_Light
                : R.style.TextAppearance_TextMediumThick_Secondary;
    }

    /**
     * Returns the primary icon tint resource to use based on the incognito state.
     *
     * @param isIncognito When true, returns the baseline light tint color res; otherwise returns
     *         the default primary icon tint list that is adaptive and can be dynamic.
     * @return The {@link ColorRes} for the icon tint.
     */
    public static @ColorRes int getPrimaryIconTintRes(boolean isIncognito) {
        return isIncognito
                ? R.color.default_icon_color_light_tint_list
                : R.color.default_icon_color_tint_list;
    }

    /**
     * Returns the primary icon tint to use based on the incognito state.
     *
     * @param context The {@link Context} used to retrieve colors.
     * @param isIncognito When true, returns the baseline light tint list; otherwise returns the
     *     default primary icon tint list that is adaptive and can be dynamic.
     * @return The {@link ColorStateList} for the icon tint.
     */
    public static ColorStateList getPrimaryIconTint(Context context, boolean isIncognito) {
        return context.getColorStateList(getPrimaryIconTintRes(isIncognito));
    }

    /**
     * Returns the secondary icon tint resource to use based on the current parameters and whether
     * the app is in night mode.
     *
     * @param forceLightIconTint When true, returns the light tint color res; otherwise returns
     *         adaptive secondary icon tint color res.
     * @return The {@link ColorRes} for the icon tint.
     */
    public static @ColorRes int getSecondaryIconTintRes(boolean forceLightIconTint) {
        return forceLightIconTint
                ? R.color.default_icon_color_secondary_light_tint_list
                : R.color.default_icon_color_secondary_tint_list;
    }

    /**
     * Returns the secondary icon tint to use based on the current parameters and whether the app is
     * in night mode.
     *
     * @param context The {@link Context} used to retrieve colors.
     * @param forceLightIconTint When true, returns the light tint color res; otherwise returns
     *     adaptive secondary icon tint color res.
     * @return The {@link ColorStateList} for the icon tint.
     */
    public static ColorStateList getSecondaryIconTint(Context context, boolean forceLightIconTint) {
        return context.getColorStateList(getSecondaryIconTintRes(forceLightIconTint));
    }

    /**
     * Get the default background color based on the incognito status.
     *
     * @param context The {@link Context} used to retrieve colors.
     * @param isIncognito When true, returns the baseline dark tint color; otherwise returns
     *     adaptive background color res.
     * @return The {@link ColorRes} for the background.
     */
    public static @ColorInt int getDefaultBgColor(Context context, boolean isIncognito) {
        if (isIncognito) {
            return context.getColor(R.color.default_bg_color_dark);
        }
        return SemanticColorUtils.getDefaultBgColor(context);
    }

    /**
     * Get the inverse background color based on the incognito status.
     *
     * @param context The {@link Context} used to retrieve colors.
     * @param isIncognito When true, returns the baseline light tint color; otherwise returns
     *     baseline dark background tint color.
     * @return The {@link ColorRes} for the background.
     */
    public static @ColorInt int getInverseBgColor(Context context, boolean isIncognito) {
        if (isIncognito) {
            return context.getColor(R.color.default_bg_color_light);
        }
        return SemanticColorUtils.getColorSurfaceInverse(context);
    }

    /** {@return The {@link ColorInt} keyboard focus ring color} */
    public static @ColorInt int getKeyboardFocusRingColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.baseline_neutral_90)
                : SemanticColorUtils.getDefaultControlColorActive(context);
    }
}
