// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;

/**
 * Provides common default colors for Chrome UI.
 */
public class ChromeColors {
    /**
     * Determines the default theme color used for toolbar based on the provided parameters.
     *
     * @param res {@link Resources} used to retrieve colors.
     * @param forceDarkBgColor When true, returns the default dark-mode color; otherwise returns
     *        adaptive default color.
     * @return The default theme color.
     */
    public static @ColorInt int getDefaultThemeColor(Resources res, boolean forceDarkBgColor) {
        return forceDarkBgColor
                ? ApiCompatibilityUtils.getColor(res, R.color.toolbar_background_primary_dark)
                : ApiCompatibilityUtils.getColor(res, R.color.toolbar_background_primary);
    }

    /**
     * Returns the primary background color used as native page background based on the given
     * parameters.
     *
     * @param res The {@link Resources} used to retrieve colors.
     * @param forceDarkBgColor When true, returns the dark-mode primary background color; otherwise
     *        returns adaptive primary background color.
     * @return The primary background color.
     */
    public static @ColorInt int getPrimaryBackgroundColor(Resources res, boolean forceDarkBgColor) {
        return forceDarkBgColor ? ApiCompatibilityUtils.getColor(res, R.color.default_bg_color_dark)
                                : ApiCompatibilityUtils.getColor(res, R.color.default_bg_color);
    }

    /**
     * Returns the large text primary style based on the given parameter.
     *
     * @param forceLightTextColor When true, returns the light-mode large text primary style;
     *         otherwise returns adaptive large text primary style.
     * @return The large text primary style.
     */
    public static int getLargeTextPrimaryStyle(boolean forceLightTextColor) {
        return forceLightTextColor ? R.style.TextAppearance_TextLarge_Primary_Light
                                   : R.style.TextAppearance_TextLarge_Primary;
    }

    /**
     * Returns the text medium thick secondary style based on the given parameter.
     *
     * @param forceLightTextColor When true, returns the light-mode medium text secondary style;
     *         otherwise returns adaptive medium text secondary style.
     * @return The medium text secondary style.
     */
    public static int getTextMediumThickSecondaryStyle(boolean forceLightTextColor) {
        return forceLightTextColor ? R.style.TextAppearance_TextMediumThick_Secondary_Light
                                   : R.style.TextAppearance_TextMediumThick_Secondary;
    }

    /**
     * Returns the primary icon tint resource to use based on the current parameters and whether
     * the app is in night mode.
     *
     * @param forceLightIconTint When true, returns the light tint color res; otherwise returns
     *         adaptive primary icon tint color res.
     * @return The {@link ColorRes} for the icon tint.
     */
    public static @ColorRes int getPrimaryIconTintRes(boolean forceLightIconTint) {
        return forceLightIconTint ? R.color.default_icon_color_light_tint_list
                                  : R.color.default_icon_color_tint_list;
    }

    /**
     * Returns the primary icon tint to use based on the current parameters and whether the app is
     * in night mode.
     *
     * @param context The {@link Context} used to retrieve colors.
     * @param forceLightIconTint When true, returns the light tint color res; otherwise returns
     *         adaptive primary icon tint color res.
     * @return The {@link ColorStateList} for the icon tint.
     */
    public static ColorStateList getPrimaryIconTint(Context context, boolean forceLightIconTint) {
        return AppCompatResources.getColorStateList(
                context, getPrimaryIconTintRes(forceLightIconTint));
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
        return forceLightIconTint ? R.color.default_icon_color_secondary_light_tint_list
                                  : R.color.default_icon_color_secondary_tint_list;
    }

    /**
     * Returns the secondary icon tint to use based on the current parameters and whether the app
     * is in night mode.
     *
     * @param context The {@link Context} used to retrieve colors.
     * @param forceLightIconTint When true, returns the light tint color res; otherwise returns
     *         adaptive secondary icon tint color res.
     * @return The {@link ColorStateList} for the icon tint.
     */
    public static ColorStateList getSecondaryIconTint(Context context, boolean forceLightIconTint) {
        return AppCompatResources.getColorStateList(
                context, getSecondaryIconTintRes(forceLightIconTint));
    }
}
