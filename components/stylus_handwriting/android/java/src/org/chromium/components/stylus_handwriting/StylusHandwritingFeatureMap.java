// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for state of Stylus Handwriting feature flags. */
@JNINamespace("stylus_handwriting::android")
public class StylusHandwritingFeatureMap extends FeatureMap {

    public static final String USE_HANDWRITING_INITIATOR = "UseHandwritingInitiator";
    private static final StylusHandwritingFeatureMap sInstance = new StylusHandwritingFeatureMap();

    // Do not instantiate this class.
    private StylusHandwritingFeatureMap() {
        super();
    }

    /**
     * @return the singleton StylusHandwritingFeatureMap.
     */
    public static StylusHandwritingFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return StylusHandwritingFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
