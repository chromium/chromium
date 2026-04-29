// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.ValueUtils;

/** Utility methods for the bottom bar. */
@NullMarked
public class BottomBarUtils {
    private static final int[][] DISABLED_AND_NORMAL_STATES =
            new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}};

    /**
     * Returns the background color for the bottom bar.
     *
     * @param context The context used to resolve the color.
     * @param brandedColorScheme The branded color scheme for the bottom bar.
     * @return The background color int.
     */
    public static @ColorInt int getBottomBarBackgroundColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito = brandedColorScheme == BrandedColorScheme.INCOGNITO;
        return IncognitoColors.getColorSurfaceContainerHigh(context, isIncognito);
    }

    /**
     * Returns the color state list for bottom bar action icons.
     *
     * @param context The context used to resolve the color.
     * @param brandedColorScheme The branded color scheme for the bottom bar.
     * @return The color state list.
     */
    public static ColorStateList getIconColorStateList(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito = brandedColorScheme == BrandedColorScheme.INCOGNITO;
        int onSurface = IncognitoColors.getColorOnSurface(context, isIncognito);
        float disabledAlpha =
                ValueUtils.getFloat(context.getResources(), R.dimen.default_disabled_alpha);
        int disabledColor = ColorUtils.setAlphaComponentWithFloat(onSurface, disabledAlpha);
        return new ColorStateList(DISABLED_AND_NORMAL_STATES, new int[] {disabledColor, onSurface});
    }
}
