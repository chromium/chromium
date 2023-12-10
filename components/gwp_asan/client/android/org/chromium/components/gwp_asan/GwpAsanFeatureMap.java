// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gwp_asan;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/**
 * Java accessor for base::Features listed in components/gwp_asan/client/feature_map.cc.
 * Patterned after example linked from `android_accessing_cpp_features_in_java.md`.
 */
@JNINamespace("gwp_asan::android")
public class GwpAsanFeatureMap extends FeatureMap {
    private static final GwpAsanFeatureMap sInstance = new GwpAsanFeatureMap();

    // Do not instantiate this class.
    private GwpAsanFeatureMap() {}

    /** @return the singleton {@link GwpAsanFeatureMap} */
    public static GwpAsanFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return GwpAsanFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
