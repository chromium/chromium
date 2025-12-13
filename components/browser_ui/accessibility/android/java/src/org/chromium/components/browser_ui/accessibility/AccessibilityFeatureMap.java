// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/** Java accessor for base::Features listed in native */
@JNINamespace("browser_ui")
@NullMarked
public class AccessibilityFeatureMap extends FeatureMap {
    public static final String ANDROID_ZOOM_INDICATOR = "AndroidZoomIndicator";

    private static final AccessibilityFeatureMap sInstance = new AccessibilityFeatureMap();

    public static final MutableFlagWithSafeDefault sAndroidZoomIndicator =
            newMutableFlagWithSafeDefault(ANDROID_ZOOM_INDICATOR, false);

    // Do not instantiate this class.
    private AccessibilityFeatureMap() {}

    /**
     * @return the singleton AccessibilityFeatureMap.
     */
    public static AccessibilityFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return AccessibilityFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return AccessibilityFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }
}
