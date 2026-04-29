// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.IncognitoColors;

/** Utility methods for the bottom bar. */
@NullMarked
public class BottomBarUtils {
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
}
