// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/**
 * Java accessor for search engines base::Features.
 *
 * <p>Note: Features must be added to the array `kFeaturesExposedToJava` in
 * //components/search_engines/android/search_engines_feature_map.cc
 */
@JNINamespace("search_engines")
public final class SearchEnginesFeatureMap extends FeatureMap {
    private static final SearchEnginesFeatureMap sInstance = new SearchEnginesFeatureMap();

    // Do not instantiate this class.
    private SearchEnginesFeatureMap() {}

    /**
     * @return the singleton SearchEnginesFeatureMap.
     */
    public static SearchEnginesFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return SearchEnginesFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
