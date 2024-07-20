// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.hasTappableBottomBar;

import android.app.Activity;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.Tab;
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
     * @param layoutManager The {@link LayoutManager} for checking the active layout type.
     * @return An EdgeToEdgeController to control drawing under System Bars, or {@code null} if this
     *     version of Android does not support the APIs needed.
     */
    public static @Nullable EdgeToEdgeController create(
            Activity activity,
            WindowAndroid windowAndroid,
            @NonNull ObservableSupplier<Tab> tabObservableSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            LayoutManager layoutManager) {
        if (Build.VERSION.SDK_INT < VERSION_CODES.R) return null;
        return new EdgeToEdgeControllerImpl(
                activity,
                windowAndroid,
                tabObservableSupplier,
                null,
                browserControlsStateProvider,
                layoutManager);
    }

    /**
     * Build the coordinator that manages the edge-to-edge bottom chin.
     *
     * @param layoutManager The {@link LayoutManager} for adding new scene overlays.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing the edge-to-edge
     *     status and window bottom insets.
     * @param navigationBarColorProvider The {@link NavigationBarColorProvider} for observing the
     *     color for the navigation bar.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     */
    public static Destroyable createBottomChin(
            LayoutManager layoutManager,
            EdgeToEdgeController edgeToEdgeController,
            NavigationBarColorProvider navigationBarColorProvider,
            BottomControlsStacker bottomControlsStacker) {
        return new EdgeToEdgeBottomChinCoordinator(
                layoutManager,
                edgeToEdgeController,
                navigationBarColorProvider,
                bottomControlsStacker);
    }

    /**
     * Creates an adjuster for padding to the view to account for edge-to-edge. Note: this doesn't
     * account for browser controls.
     *
     * @param view The view to be adjusted.
     */
    public static EdgeToEdgePadAdjuster createForView(View view) {
        return new SimpleEdgeToEdgePadAdjuster(view, /* accountForBrowserControls= */ false);
    }

    /**
     * Creates an adjuster for padding to the view to account for edge-to-edge.
     *
     * @param view The view to be adjusted.
     * @param accountForBrowserControls Whether to account for browser controls when adjusting the
     *     view.
     */
    public static EdgeToEdgePadAdjuster createForView(
            View view, boolean accountForBrowserControls) {
        return new SimpleEdgeToEdgePadAdjuster(view, accountForBrowserControls);
    }

    /**
     * @return whether the configuration of the device should allow Edge To Edge.
     */
    public static boolean isSupportedConfiguration(Activity activity) {
        // Make sure we test SDK version before checking the Feature so Field Trials only collect
        // from qualifying devices.
        if (android.os.Build.VERSION.SDK_INT < VERSION_CODES.R) return false;

        boolean atLeastOneE2EFeatureEnabled =
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled()
                        || EdgeToEdgeUtils.isFullWebEdgeToEdgeOptInEnabled()
                        || EdgeToEdgeUtils.isEnabled();

        return atLeastOneE2EFeatureEnabled
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                && !BuildInfo.getInstance().isAutomotive
                // TODO(https://crbug.com/325356134) use UiUtils#isGestureNavigationMode instead.
                && !hasTappableBottomBar(activity.getWindow())
                && !sHas3ButtonNavBarForTesting;
    }

    @VisibleForTesting
    public static void setHas3ButtonNavBar(boolean has3ButtonNavBar) {
        sHas3ButtonNavBarForTesting = has3ButtonNavBar;
    }
}
