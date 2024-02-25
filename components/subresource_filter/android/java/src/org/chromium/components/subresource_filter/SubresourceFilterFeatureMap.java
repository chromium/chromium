// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.subresource_filter;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/**
 * Java accessor for subresource_filter base::Features.
 *
 * Note: Features must be added to the array |kFeaturesExposedToJava| in
 * //components/subresource_filter/core/browser/subresource_filter_feature_map.cc.
 */
@JNINamespace("subresource_filter")
public final class SubresourceFilterFeatureMap extends FeatureMap {
    // Features exposed through this FeatureMap
    public static final String SUBRESOURCE_FILTER = "SubresourceFilter";

    private static final SubresourceFilterFeatureMap sInstance = new SubresourceFilterFeatureMap();

    // Do not instantiate this class.
    private SubresourceFilterFeatureMap() {}

    /** @return the singleton SubresourceFilterFeatureMap. */
    public static SubresourceFilterFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    /** @return whether SubresourceFilter is enabled. */
    public static boolean isSubresourceFilterEnabled() {
        return isEnabled(SUBRESOURCE_FILTER);
    }

    @Override
    protected long getNativeMap() {
        return SubresourceFilterFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
