// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.hasTappableBottomBar;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled;

import android.app.Activity;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.edge_to_edge.SystemBarColorHelper;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

/**
 * Creates an {@link EdgeToEdgeController} used to control drawing using the Android Edge to Edge
 * Feature. This allows drawing under Android System Bars.
 */
public class EdgeToEdgeControllerFactory {
    private static boolean sHas3ButtonNavBarForTesting;

    /**
     * Creates an {@link EdgeToEdgeController} instance using the given activity and {@link
     * ObservableSupplier} for a Tab.
     *
     * @param activity The Android {@link Activity} to allow drawing under System Bars.
     * @param windowAndroid The current {@link WindowAndroid} to allow drawing under System Bars.
     * @param tabObservableSupplier Supplies an {@Link Observer} that is notified whenever the Tab
     *     changes.
     * @param browserControlsStateProvider Provides the state of the BrowserControls so we can tell
     *     if the Toolbar is changing.
     * @param layoutManagerSupplier The supplier of {@link LayoutManager} for checking the active
     *     layout type.
     * @return An EdgeToEdgeController to control drawing under System Bars, or {@code null} if this
     *     version of Android does not support the APIs needed.
     */
    public static @Nullable EdgeToEdgeController create(
            Activity activity,
            WindowAndroid windowAndroid,
            @NonNull ObservableSupplier<Tab> tabObservableSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager) {
        if (Build.VERSION.SDK_INT < VERSION_CODES.R) return null;
        assert isSupportedConfiguration(activity);
        return new EdgeToEdgeControllerImpl(
                activity,
                windowAndroid,
                tabObservableSupplier,
                null,
                browserControlsStateProvider,
                layoutManagerSupplier,
                fullscreenManager);
    }

    /**
     * Build the coordinator that manages the edge-to-edge bottom chin.
     *
     * @param androidView The Android view for the bottom chin.
     * @param keyboardVisibilityDelegate A {@link KeyboardVisibilityDelegate} for watching keyboard
     *     visibility events.
     * @param layoutManager The {@link LayoutManager} for adding new scene overlays.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing the edge-to-edge
     *     status and window bottom insets.
     * @param navigationBarColorProvider The {@link NavigationBarColorProvider} for observing the
     *     color for the navigation bar.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     * @param fullscreenManager The {@link FullscreenManager} for provide the fullscreen state.
     */
    public static SystemBarColorHelper createBottomChin(
            View androidView,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            LayoutManager layoutManager,
            EdgeToEdgeController edgeToEdgeController,
            NavigationBarColorProvider navigationBarColorProvider,
            BottomControlsStacker bottomControlsStacker,
            FullscreenManager fullscreenManager) {
        assert isEdgeToEdgeBottomChinEnabled();
        return new EdgeToEdgeBottomChinCoordinator(
                androidView,
                keyboardVisibilityDelegate,
                layoutManager,
                edgeToEdgeController,
                navigationBarColorProvider,
                bottomControlsStacker,
                fullscreenManager);
    }

    /**
     * Creates an adjuster for padding to the view to account for edge-to-edge. Note: this doesn't
     * account for browser controls.
     *
     * @param view The view to be adjusted.
     */
    public static EdgeToEdgePadAdjuster createForView(View view) {
        return new SimpleEdgeToEdgePadAdjuster(
                view, EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled());
    }

    /**
     * Creates an adjuster for padding to the view to account for edge-to-edge, and observe the
     * supplier if edge to edge is enabled.
     *
     * @param view The view to be adjusted.
     * @param edgeToEdgeControllerSupplier Supplier to the {@link EdgeToEdgeController}.
     */
    public static EdgeToEdgePadAdjuster createForViewAndObserveSupplier(
            View view,
            @Nullable ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        return new SimpleEdgeToEdgePadAdjuster(
                view,
                edgeToEdgeControllerSupplier,
                EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled());
    }

    /**
     * Returns whether the configuration of the device should allow Edge To Edge. Note the results
     * are false-positive, if the method is called before the |activity|'s decor view being attached
     * to the window.
     */
    public static boolean isSupportedConfiguration(Activity activity) {
        // Make sure we test SDK version before checking the Feature so Field Trials only collect
        // from qualifying devices.
        if (!EdgeToEdgeFieldTrial.getInstance().isEnabledForManufacturerVersion()) return false;

        // The root view's window insets is needed to determine if we are in gesture nav mode.
        if (activity.getWindow().getDecorView().getRootWindowInsets() == null) {
            return false;
        }

        return EdgeToEdgeUtils.isEnabled()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                && !BuildInfo.getInstance().isAutomotive
                // TODO(https://crbug.com/325356134) use UiUtils#isGestureNavigationMode instead.
                && !hasTappableBottomBar(activity.getWindow())
                && !sHas3ButtonNavBarForTesting;
    }

    @VisibleForTesting
    public static void setHas3ButtonNavBar(boolean has3ButtonNavBar) {
        sHas3ButtonNavBarForTesting = has3ButtonNavBar;
        ResettersForTesting.register(() -> sHas3ButtonNavBarForTesting = false);
    }
}
