// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for interactions with ThemeOverlayProvider. */
@NullMarked
public final class ThemeModuleUtils {

    private static @Nullable ThemeOverlayProvider sInstance;

    private ThemeModuleUtils() {}

    /** Returns whether theme module is enabled. */
    public static boolean isEnabled() {
        return ChromeFeatureList.sAndroidThemeModule.isEnabled();
    }

    /** Returns whether enable all the dependency features. */
    public static boolean isForceEnableDependencies() {
        return isEnabled() && ChromeFeatureList.sAndroidThemeModuleForceDependencies.getValue();
    }

    /**
     * Get the theme module to provide the overlay. This will be a theme resource used for {@link
     * android.content.res.Resources.Theme#applyStyle(int, boolean)}.
     */
    public static ThemeOverlayProvider getProviderInstance() {
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(ThemeOverlayProvider.class);
            if (sInstance == null) {
                sInstance = new EmptyThemeOverlayProvider();
            }
        }
        return sInstance;
    }

    /** Override the provider instance with a test object. */
    public static void setProviderInstanceForTesting(@Nullable ThemeOverlayProvider provider) {
        ThemeOverlayProvider instance = sInstance;
        sInstance = provider;
        ResettersForTesting.register(() -> sInstance = instance);
    }

    private static class EmptyThemeOverlayProvider implements ThemeOverlayProvider {
        @Override
        public int getThemeOverlay() {
            return 0;
        }
    }
}
