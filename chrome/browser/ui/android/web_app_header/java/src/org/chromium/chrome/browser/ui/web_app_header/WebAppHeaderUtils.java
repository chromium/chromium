// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.os.Build;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Provides predicate and histogram utilities related to webapp header. */
@NullMarked
public class WebAppHeaderUtils {

    @IntDef({
        ReloadType.INVALID,
        ReloadType.STOP_RELOAD,
        ReloadType.RELOAD_FROM_CACHE,
        ReloadType.RELOAD_IGNORE_CACHE,
        ReloadType.MAX_VALUE
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface ReloadType {
        /**
         * Indicates reload attempt when tab is null, e.g. navigating to tab switcher or during tab
         * re-parenting.
         */
        int INVALID = 0;

        /** Indicates stop reloading attempt on currently visible tab. */
        int STOP_RELOAD = 1;

        /**
         * Indicates reload attempt from cache (left click or touch without meta keys) on currently
         * visible tab.
         */
        int RELOAD_FROM_CACHE = 2;

        /**
         * Indicated reload attempt ignoring cache (left click with meta key) on currently visible
         * tab
         */
        int RELOAD_IGNORE_CACHE = 3;

        /**
         * Maximum number of enum entries. This value should be updated when new entries are added.
         * Do not decrement!
         */
        int MAX_VALUE = RELOAD_IGNORE_CACHE;
    }

    @IntDef({BackEvent.INVALID, BackEvent.BACK, BackEvent.NAVIGATION_MENU, BackEvent.MAX_VALUE})
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface BackEvent {
        /**
         * Indicates back press attempt when tab is null, e.g. navigating to tab switcher or during
         * tab re-parenting.
         */
        int INVALID = 0;

        /** Indicates single back press attempt on current active tab. */
        int BACK = 1;

        /** Indicated long back press that opens a navigation pop up on current active tab. */
        int NAVIGATION_MENU = 2;

        /**
         * Maximum number of enum entries. This value should be updated when new entries are added.
         * Do not decrement!
         */
        int MAX_VALUE = NAVIGATION_MENU;
    }

    private WebAppHeaderUtils() {}

    /**
     * Checks whether minimal ui for large screens is enabled. This includes checking feature flag
     * and type of web app that's running right now.
     *
     * @param intentDataProvider contains intent data related to the current browser service.
     * @return true when minimal ui flag is enabled and currently running a web app, otherwise
     *     false.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static boolean isMinimalUiEnabled(BrowserServicesIntentDataProvider intentDataProvider) {
        @DisplayMode.EnumType int displayMode = intentDataProvider.getResolvedDisplayMode();

        boolean isTrustedWebApp =
                intentDataProvider.isWebApkActivity() || intentDataProvider.isTrustedWebActivity();
        return isTrustedWebApp && displayMode == DisplayMode.MINIMAL_UI && isMinimalUiFlagEnabled();
    }

    /**
     * Checks whether standalone display mode is enabled. This includes checking that the the
     * standalone feature flag is enabled, as well as the type of web app of the intent.
     *
     * @param intentDataProvider contains intent data related to the current browser service.
     * @return true when currently running a trusted web app in standalone mode, else return false.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static boolean isStandaloneEnabled(
            BrowserServicesIntentDataProvider intentDataProvider) {
        @DisplayMode.EnumType int displayMode = intentDataProvider.getResolvedDisplayMode();

        return intentDataProvider.isTrustedWebActivity()
                && displayMode == DisplayMode.STANDALONE
                && ChromeFeatureList.sAndroidWebAppHeaderForStandaloneMode.isEnabled();
    }

    /**
     * Checks whether minimal ui feature flag is enabled on Android 15+ SDK.
     *
     * @return true if flag is enabled and running on SDK >= 35, false otherwise.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static boolean isMinimalUiFlagEnabled() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && ChromeFeatureList.sAndroidMinimalUiLargeScreen.isEnabled();
    }

    /**
     * Checks whether window controls overlay is enabled. This includes checking feature flag and
     * type of web app that's running right now.
     *
     * @param intentDataProvider contains intent data related to the current browser service.
     * @return true when window controls overla flag is enabled and currently running a TWA,
     *     otherwise false.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static boolean isWindowControlsOverlayEnabled(
            BrowserServicesIntentDataProvider intentDataProvider) {
        @DisplayMode.EnumType int displayMode = intentDataProvider.getResolvedDisplayMode();

        return intentDataProvider.isTrustedWebActivity()
                && displayMode == DisplayMode.WINDOW_CONTROLS_OVERLAY
                && isWindowControlsOverlayFlagEnabled();
    }

    /** Checks whether the window controls overlay feature flag is enabled. */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static boolean isWindowControlsOverlayFlagEnabled() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && ChromeFeatureList.sAndroidWindowControlsOverlay.isEnabled();
    }

    /** Checks whether a display mode that requires the custom web app header is enabled. */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static boolean isWebAppHeaderEnabled(
            BrowserServicesIntentDataProvider intentDataProvider) {
        return isMinimalUiEnabled(intentDataProvider)
                || isStandaloneEnabled(intentDataProvider)
                || isWindowControlsOverlayEnabled(intentDataProvider);
    }

    /**
     * Provides layout id of the webapp header.
     *
     * @return webapp header layout resource id.
     */
    public static @LayoutRes int getWebAppHeaderLayoutId() {
        return R.layout.web_app_main_layout;
    }

    /**
     * Provides layout id of the webapp content.
     *
     * @return webapp content resource id.
     */
    public static int getWebAppHeaderContentId() {
        return R.id.web_app_content;
    }

    /**
     * Provides layout id of the webapp find toolbar tablet stub.
     *
     * @return webapp find toolbar tablet stub id.
     */
    public static int getWebAppHeaderFindToolbarTabletId() {
        return R.id.web_app_header_find_toolbar_tablet_stub;
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
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
        return isMinimalUiEnabled(intentDataProvider)
                && AppHeaderUtils.isAppInDesktopWindow(desktopWindowStateManager);
    }

    /**
     * Records reload button events.
     *
     * @param type event {@link ReloadType}.
     */
    static void recordReloadButtonEvent(@ReloadType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.WebAppHeader.ReloadButtonEvent", type, ReloadType.MAX_VALUE);
    }

    /**
     * Records back button events.
     *
     * @param type {@link BackEvent} type.
     */
    static void recordBackButtonEvent(@BackEvent int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.WebAppHeader.BackButtonEvent", type, ReloadType.MAX_VALUE);
    }
}
