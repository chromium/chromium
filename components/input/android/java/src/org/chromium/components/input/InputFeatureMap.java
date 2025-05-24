// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.input;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;

/** Java accessor for state of Input feature flags. */
@JNINamespace("input")
@NullMarked
public class InputFeatureMap extends FeatureMap {
    public static final String USE_ANDROID_BUFFERED_INPUT_DISPATCH =
            "UseAndroidBufferedInputDispatch";

    private static final InputFeatureMap sInstance = new InputFeatureMap();

    // Do not instantiate this class.
    private InputFeatureMap() {
        super();
    }

    /**
     * @return the singleton {@link InputFeatureMap}.
     */
    public static InputFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return InputFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    interface Natives {
        long getNativeMap();
    }
}
