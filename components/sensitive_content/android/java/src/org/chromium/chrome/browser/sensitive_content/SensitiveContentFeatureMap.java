// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sensitive_content;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for state of sensitive content features. */
@JNINamespace("sensitive_content")
public class SensitiveContentFeatureMap extends FeatureMap {
    private static final SensitiveContentFeatureMap sInstance = new SensitiveContentFeatureMap();

    // Do not instantiate this class.
    private SensitiveContentFeatureMap() {
        super();
    }

    /**
     * @return the singleton SensitiveContentFeatureMap.
     */
    private static SensitiveContentFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return SensitiveContentFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
