// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.cached_flags.CachedFlag;

/** Java accessor for state of Omnibox feature flags. */
@JNINamespace("omnibox::android")
public class OmniboxFeatureMap extends FeatureMap {
    private static OmniboxFeatureMap sInstance;

    // Not directly instantiable.
    private OmniboxFeatureMap() {
        super();
    }

    /**
     * @return the singleton OmniboxFeatureMap.
     */
    private static OmniboxFeatureMap getInstance() {
        if (sInstance == null) sInstance = new OmniboxFeatureMap();
        return sInstance;
    }

    public static CachedFlag newCachedFlag(String featureName, boolean defaultValue) {
        return new CachedFlag(OmniboxFeatureMap.getInstance(), featureName, defaultValue);
    }

    public static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return OmniboxFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }

    @Override
    protected long getNativeMap() {
        return OmniboxFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    protected interface Natives {
        long getNativeMap();
    }
}
