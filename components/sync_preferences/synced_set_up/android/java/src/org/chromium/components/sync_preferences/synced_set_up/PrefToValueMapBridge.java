// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync_preferences.synced_set_up;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

import java.util.HashMap;
import java.util.Map;

/**
 * A JNI bridge for mapping preference keys to values. This class is created and owned by Java, and
 * it owns its C++ counterpart. The C++ code can populate the map with values.
 */
@NullMarked
@JNINamespace("sync_preferences::synced_set_up")
public class PrefToValueMapBridge {

    private long mNativePrefToValueMapBridge;
    private final Map<String, Object> mPrefsToValues = new HashMap<>();

    /** Creates a new PrefToValueMapBridge and its native counterpart. */
    public PrefToValueMapBridge() {
        mNativePrefToValueMapBridge = PrefToValueMapBridgeJni.get().init(this);
    }

    public long getNativeBridgePtr() {
        return mNativePrefToValueMapBridge;
    }

    /** Destroys the native counterpart. */
    public void destroy() {
        if (mNativePrefToValueMapBridge != 0) {
            PrefToValueMapBridgeJni.get().destroy(mNativePrefToValueMapBridge);
            mNativePrefToValueMapBridge = 0;
        }
    }

    @CalledByNative
    private void putStringValue(
            @JniType("std::string") String key, @JniType("std::string") String value) {
        mPrefsToValues.put(key, value);
    }

    @CalledByNative
    private void putIntValue(@JniType("std::string") String key, int value) {
        mPrefsToValues.put(key, value);
    }

    @CalledByNative
    private void putBooleanValue(@JniType("std::string") String key, boolean value) {
        mPrefsToValues.put(key, value);
    }

    /**
     * @return The map of preference keys to values.
     */
    public Map<String, Object> getPrefValueMap() {
        return mPrefsToValues;
    }

    @NativeMethods
    interface Natives {
        long init(PrefToValueMapBridge caller);

        void destroy(long nativePrefToValueMapBridge);
    }
}
