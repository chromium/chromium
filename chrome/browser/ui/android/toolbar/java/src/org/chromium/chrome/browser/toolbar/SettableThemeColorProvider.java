// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;

import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/**
 * {@link ThemeColorProvider} that tracks whatever primary color it's set to. It contains no actual
 * tracking logic; to function properly, setPrimaryColor must be called each time the color changes.
 */
@Deprecated
class SettableThemeColorProvider extends ThemeColorProvider {
    /**
     * @param context The {@link Context} that is used to retrieve color related resources.
     */
    public SettableThemeColorProvider(Context context) {
        super(context);
    }

    /** Sets the primary color to the specified value. */
    public void setPrimaryColor(int color, boolean shouldAnimate) {
        updatePrimaryColor(color, shouldAnimate);
    }

    /** Sets the tint to the specified value. */
    public void setTint(ColorStateList tint, @BrandedColorScheme int brandedColorScheme) {
        updateTint(tint, tint, brandedColorScheme);
    }
}
