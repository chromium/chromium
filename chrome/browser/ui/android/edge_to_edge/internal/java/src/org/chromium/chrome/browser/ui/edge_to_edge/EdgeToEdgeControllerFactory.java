// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.os.Build;
import android.os.Build.VERSION_CODES;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Creates an {@link EdgeToEdgeController} used to control drawing using the Android Edge to Edge
 * Feature. This allows drawing under Android System Bars.
 */
public class EdgeToEdgeControllerFactory {
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

    /** @Return whether the feature is enabled or not. */
    public static boolean isEnabled() {
        // Make sure we test SDK version before checking the Feature so Field Trials only collect
        // from qualifying devices.
        if (android.os.Build.VERSION.SDK_INT < VERSION_CODES.R) return false;
        return ChromeFeatureList.sDrawEdgeToEdge.isEnabled();
    }
}
