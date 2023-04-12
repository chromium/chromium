// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.os.Build.VERSION_CODES;

import androidx.appcompat.app.AppCompatActivity;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Creates an {@link EdgeToEdgeController} used to control drawing using the Android Edge to Edge
 * Feature. This allows drawing under Android System Bars.
 */
public class EdgeToEdgeControllerFactory {
    /** Creates an {@link EdgeToEdgeController} instance using the given activity. */
    public static EdgeToEdgeController create(AppCompatActivity activity) {
        return new EdgeToEdgeControllerImpl(activity);
    }

    /** @Return whether the feature is enabled or not. */
    public static boolean isEnabled() {
        // Make sure we test SDK version before checking the Feature so Field Trials only collect
        // from qualifying devices.
        if (android.os.Build.VERSION.SDK_INT < VERSION_CODES.R) return false;
        return ChromeFeatureList.isEnabled(ChromeFeatureList.DRAW_EDGE_TO_EDGE);
    }
}
