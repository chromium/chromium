// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection.bridges;

import android.net.Uri;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.offline_items_collection.OfflineItemShareInfo;

/**
 * The Java counterpart to the C++ class OfflineItemShareInfoBridge
 * (components/offline_items_collection/core/android/offline_item_share_info_bridge.h).  This class
 * has no public members or methods and is meant as a private factory to build
 * {@link OfflineItemShareInfo} instances.
 */
@JNINamespace("offline_items_collection::android")
public final class OfflineItemShareInfoBridge {
    private OfflineItemShareInfoBridge() {}

    /**
     * This is a helper method to allow C++ to create an {@link OfflineItemShareInfo} object.
     * @return The newly created {@link OfflineItemShareInfo} based on the input parameters.
     */
    @CalledByNative
    private static OfflineItemShareInfo createOfflineItemShareInfo(String uri) {
        OfflineItemShareInfo shareInfo = new OfflineItemShareInfo();
        if (!TextUtils.isEmpty(uri)) shareInfo.uri = Uri.parse(uri);
        return shareInfo;
    }
}
