// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync_preferences.synced_set_up;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.HashMap;
import java.util.Map;

/**
 * A JNI bridge for mapping preference keys to values. This class is created and owned by Java, and
 * it owns its C++ counterpart. The C++ code can populate the map with values.
 */
@NullMarked
@JNINamespace("sync_preferences::synced_set_up")
public class PrefToValueMapBridge {
    @JniType("::sync_preferences::synced_set_up::PrefToValueMapBridge")
    public interface NativePrefToValueMapBridge extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativePrefToValueMapBridge> mNativePrefToValueMapBridge;
    private final Map<String, Object> mPrefsToValues = new HashMap<>();

    /** Creates a new PrefToValueMapBridge and its native counterpart. */
    public PrefToValueMapBridge() {
        mNativePrefToValueMapBridge = PrefToValueMapBridgeJni.get().init(this);
    }

    public JniPtr<NativePrefToValueMapBridge> getNativeBridgePtr() {
        if (mNativePrefToValueMapBridge == null) {
            throw new IllegalStateException("PrefToValueMapBridge has already been destroyed.");
        }
        return mNativePrefToValueMapBridge;
    }

    /** Destroys the native counterpart. */
    public void destroy() {
        if (mNativePrefToValueMapBridge != null) {
            mNativePrefToValueMapBridge.destroy();
            mNativePrefToValueMapBridge = null;
        }
    }

    @CalledByNative
    @VisibleForTesting
    void putStringValue(
            @JniType("std::string_view") String key, @JniType("std::string_view") String value) {
        mPrefsToValues.put(key, value);
    }

    @CalledByNative
    @VisibleForTesting
    void putIntValue(@JniType("std::string_view") String key, int value) {
        mPrefsToValues.put(key, value);
    }

    @CalledByNative
    @VisibleForTesting
    void putBooleanValue(@JniType("std::string_view") String key, boolean value) {
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
        JniUniquePtr<NativePrefToValueMapBridge> init(PrefToValueMapBridge caller);
    }
}
