// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** Utility class that provides color values based on feature flags enabled. */
@NullMarked
public class SurfaceColorUpdateUtils {
    /**
     * Returns the background color for the Omnibox based on the enabled flag.
     *
     * @param context {@link Context} used to retrieve colors.
     * @return The background color.
     */
    public static @ColorInt int getOmniboxBackgroundColor(Context context, boolean isIncognito) {
        return ContextCompat.getColor(context, R.color.toolbar_text_box_bg_color);
    }

    /**
     * Returns the background color for the toolbar based on the enabled flag and other parameters.
     *
     * @param context {@link Context} used to retrieve colors.
     * @return The background color.
     */
    public static @ColorInt int getDefaultThemeColor(Context context, boolean isIncognito) {
        return ChromeColors.getDefaultThemeColor(context, isIncognito);
    }
}
