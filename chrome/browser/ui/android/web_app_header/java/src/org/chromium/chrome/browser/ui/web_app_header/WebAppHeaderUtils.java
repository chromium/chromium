// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.os.Build;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.LayoutRes;

import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;

/** Provides predicate utilities related to webapp header. */
@NullMarked
public class WebAppHeaderUtils {
    private WebAppHeaderUtils() {}

    /**
     * Checks whether minimal ui for large screens is enabled.
     *
     * @param intentDataProvider contains intent data related to the current browser service.
     * @return true when minimal ui flag is enabled and currently running a web app, otherwise
     *     false.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static boolean isMinimalUiEnabled(
            final BrowserServicesIntentDataProvider intentDataProvider) {
        final var webAppExtras = intentDataProvider.getWebappExtras();
        if (webAppExtras == null) return false;

        final boolean isValidDisplayMode =
                webAppExtras.displayMode == DisplayMode.MINIMAL_UI
                        || webAppExtras.displayMode == DisplayMode.BROWSER;
        final boolean isTrustedWebApp =
                intentDataProvider.isWebApkActivity() || intentDataProvider.isTrustedWebActivity();
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && ChromeFeatureList.sAndroidMinimalUiLargeScreen.isEnabled()
                && isTrustedWebApp
                && isValidDisplayMode;
    }

    /**
     * Checks whether minimal ui is visible based on the desktop window state and feature flag
     * state.
     *
     * @param intentDataProvider provides information about web app.
     * @param desktopWindowStateManager provides desktop windowing state.
     * @return true when desktop minimal ui is visible, false otherwise.
     */
    public static boolean isMinimalUiVisible(
            BrowserServicesIntentDataProvider intentDataProvider,
            DesktopWindowStateManager desktopWindowStateManager) {
        return isMinimalUiEnabled(intentDataProvider)
                && AppHeaderUtils.isAppInDesktopWindow(desktopWindowStateManager);
    }

    /**
     * Provides layout id of the webapp header.
     *
     * @return webapp header layout resource id.
     */
    public static @LayoutRes int getWebAppHeaderLayoutId() {
        return R.layout.web_app_main_layout;
    }
}
