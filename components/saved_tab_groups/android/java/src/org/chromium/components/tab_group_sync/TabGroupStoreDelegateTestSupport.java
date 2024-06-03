// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Helper class for testing that provides functionality for clearing out {@link
 * TabGroupMetadataPersistentStore} over JNI.
 */
@JNINamespace("tab_groups")
public class TabGroupStoreDelegateTestSupport {
    @CalledByNative
    private static void clearTabGroupMetadataPeristentStore() {
        TabGroupMetadataPersistentStore.clearAllData();
    }
}
