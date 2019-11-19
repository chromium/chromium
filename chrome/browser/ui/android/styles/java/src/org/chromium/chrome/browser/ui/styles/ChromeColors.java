// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.styles;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.support.v7.content.res.AppCompatResources;

import androidx.annotation.ColorRes;

import org.chromium.base.ApiCompatibilityUtils;

/**
 * Provides common default colors for Chrome UI.
 */
public class ChromeColors {
    /**
     * Determines the default theme color used for toolbar based on the provided parameters.
     * @param res {@link Resources} used to retrieve colors.
     * @param useDark If true, returns a default dark color. If false, returns a default color
     *         depend
     *              on whether Night Mode is on.
     * @return The default theme color.
     */
    public static int getDefaultThemeColor(Resources res, boolean useDark) {
        return useDark
                ? ApiCompatibilityUtils.getColor(res, R.color.toolbar_background_primary_dark)
                : ApiCompatibilityUtils.getColor(res, R.color.toolbar_background_primary);
    }

    /**
     * Returns the primary background color used as native page background based on the given
     * parameters.
     * @param res The {@link Resources} used to retrieve colors.
     * @param useDark Whether or not the color is for a dark mode. If true, a dark color will be
     *               returned. Otherwise, the color returned will depend on whether Night Mode is
     *               on.
     * @return The primary background color.
     */
    public static int getPrimaryBackgroundColor(Resources res, boolean useDark) {
        return useDark
                ? ApiCompatibilityUtils.getColor(res, org.chromium.ui.R.color.dark_primary_color)
                : ApiCompatibilityUtils.getColor(res, org.chromium.ui.R.color.modern_primary_color);
    }

    /**
     * Returns the icon tint resource to use based on the current parameters and whether the app is
     * in night mode.
     * @param useLight Whether or not the icon tint should be light when not in night mode.
     * @return The {@link ColorRes} for the icon tint.
     */
    public static @ColorRes int getIconTintRes(boolean useLight) {
        return useLight ? R.color.tint_on_dark_bg : R.color.standard_mode_tint;
    }

    /**
     * Returns the icon tint to use based on the current parameters and whether the app is in night
     * mode.
     * @param context The {@link Context} used to retrieve colors.
     * @param useLight Whether or not the icon tint should be light when not in night mode.
     * @return The {@link ColorStateList} for the icon tint.
     */
    public static ColorStateList getIconTint(Context context, boolean useLight) {
        return AppCompatResources.getColorStateList(context, getIconTintRes(useLight));
    }
}
