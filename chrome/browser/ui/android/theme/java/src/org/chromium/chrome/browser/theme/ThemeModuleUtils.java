// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for the theme module feature. */
@NullMarked
public final class ThemeModuleUtils {
    private ThemeModuleUtils() {}

    /** Returns whether theme module is enabled. */
    public static boolean isEnabled() {
        return ChromeFeatureList.sAndroidThemeModule.isEnabled();
    }

    /** Returns whether enable all the dependency features. */
    public static boolean isForceEnableDependencies() {
        return isEnabled() && ChromeFeatureList.sAndroidThemeModuleForceDependencies.getValue();
    }
}
