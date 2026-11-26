// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync_preferences.synced_set_up;

import org.jni_zero.JniUniquePtr;

import org.chromium.build.annotations.NullMarked;

import java.util.function.Consumer;

/** Test helpers for stubbing {@link PrefToValueMapBridge} JNI from cross-package unit tests. */
@NullMarked
public class PrefToValueMapBridgeTestSupport {
    private static long sNextFakePtr = 1L;

    public static void stubInitForTesting(Consumer<PrefToValueMapBridge> onInit) {
        PrefToValueMapBridgeJni.setInstanceForTesting(
                caller -> {
                    onInit.accept(caller);
                    return JniUniquePtr.createForTesting(sNextFakePtr++);
                });
    }

    public static void putStringForTesting(PrefToValueMapBridge bridge, String key, String value) {
        bridge.putStringValue(key, value);
    }

    public static void putIntForTesting(PrefToValueMapBridge bridge, String key, int value) {
        bridge.putIntValue(key, value);
    }

    public static void putBooleanForTesting(
            PrefToValueMapBridge bridge, String key, boolean value) {
        bridge.putBooleanValue(key, value);
    }
}
