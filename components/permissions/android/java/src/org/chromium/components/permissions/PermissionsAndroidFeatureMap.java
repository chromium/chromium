// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for base::Features listed in {@link PermissionsAndroidFeatureList} */
@JNINamespace("permissions")
public final class PermissionsAndroidFeatureMap extends FeatureMap {
    private static final PermissionsAndroidFeatureMap sInstance =
            new PermissionsAndroidFeatureMap();

    // Do not instantiate this class.
    private PermissionsAndroidFeatureMap() {}

    /** @return the singleton DeviceFeatureMap. */
    public static PermissionsAndroidFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return PermissionsAndroidFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
