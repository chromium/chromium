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
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.DeviceFormFactor;

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
     * @param activity The Android {@link Activity}
     * @param tabObservableSupplier Supplies an {@Link Observer} that is notified whenever the Tab
     *     changes.
     * @return An EdgeToEdgeController to control drawing under System Bars, or {@code null} if this
     *     version of Android does not support the APIs needed.
     */
    public static @Nullable EdgeToEdgeController create(
            Activity activity, @NonNull ObservableSupplier<Tab> tabObservableSupplier) {
        if (Build.VERSION.SDK_INT < VERSION_CODES.R) return null;
        return new EdgeToEdgeControllerImpl(activity, tabObservableSupplier, null);
    }

    public static EdgeToEdgePadAdjuster createForView(View view) {
        return new SimpleEdgeToEdgePadAdjuster(view);
    }

    /**
     * @return whether the feature is enabled or not.
     */
    public static boolean isEnabled() {
        // Make sure we test SDK version before checking the Feature so Field Trials only collect
        // from qualifying devices.
        if (android.os.Build.VERSION.SDK_INT < VERSION_CODES.R) return false;
        return ChromeFeatureList.sDrawEdgeToEdge.isEnabled();
    }

    /**
     * @return whether the configuration of the device should allow Edge To Edge.
     */
    public static boolean isSupportedConfiguration(Activity activity) {
        if (android.os.Build.VERSION.SDK_INT < VERSION_CODES.R) return false;
        return isEnabled()
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
