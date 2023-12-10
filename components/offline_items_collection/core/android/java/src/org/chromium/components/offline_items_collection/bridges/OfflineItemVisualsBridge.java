// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection.bridges;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.offline_items_collection.OfflineItemVisuals;

/**
 * The Java counterpart to the C++ class OfflineItemVisualsBridge
 * (components/offline_items_collection/core/android/offline_item_visuals_bridge.h).  This class has
 * no public members or methods and is meant as a private factory to build
 * {@link OfflineItemVisuals} instances.
 */
@JNINamespace("offline_items_collection::android")
public final class OfflineItemVisualsBridge {
    private OfflineItemVisualsBridge() {}

    /**
     * This is a helper method to allow C++ to create an {@link OfflineItemVisuals} object.
     * @return The newly created {@link OfflineItemVisuals} based on the input parameters.
     */
    @CalledByNative
    private static OfflineItemVisuals createOfflineItemVisuals(Bitmap icon) {
        OfflineItemVisuals visuals = new OfflineItemVisuals();
        visuals.icon = icon;
        return visuals;
    }
}
