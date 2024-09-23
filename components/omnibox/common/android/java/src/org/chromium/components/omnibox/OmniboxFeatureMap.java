// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

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
    public static OmniboxFeatureMap getInstance() {
        if (sInstance == null) sInstance = new OmniboxFeatureMap();
        return sInstance;
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
