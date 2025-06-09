// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

@NullMarked
public class ToolbarResourceUtils {

    private ToolbarResourceUtils() {}

    public static @DrawableRes int backgroundResForThemeColor(
            int brandedThemeColor, boolean isWebApp) {
        boolean incognito = brandedThemeColor == BrandedColorScheme.INCOGNITO;

        if (incognito && isWebApp) {
            return org.chromium.chrome.browser.toolbar.R.drawable.small_icon_background_baseline;
        }
        if (incognito /* && !isWebApp */) {
            return org.chromium.chrome.browser.toolbar.R.drawable.default_icon_background_baseline;
        }
        if (isWebApp /* && !incognito */) {
            return org.chromium.chrome.browser.toolbar.R.drawable.small_icon_background;
        }
        /* !incognito && !isWebApp */
        return org.chromium.chrome.browser.toolbar.R.drawable.default_icon_background;
    }
}
