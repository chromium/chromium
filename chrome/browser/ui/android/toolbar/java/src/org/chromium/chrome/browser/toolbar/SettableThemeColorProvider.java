// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;

import org.chromium.chrome.browser.theme.ThemeColorProvider;

/**
 * {@link ThemeColorProvider} that blindly tracks whatever primary color it's set to.
 * It contains no actual tracking logic; to function properly, setPrimaryColor must be called each
 * time the color changes.
 */
@Deprecated
class SettableThemeColorProvider extends ThemeColorProvider {
    /**
     * @param context The {@link Context} that is used to retrieve color related resources.
     */
    public SettableThemeColorProvider(Context context) {
        super(context);
    }

    /**
     * Sets the primary color to the specified value.
     */
    public void setPrimaryColor(int color, boolean shouldAnimate) {
        updatePrimaryColor(color, shouldAnimate);
    }
}
