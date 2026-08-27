// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/** Java accessor for base::Features related to the bottom sheet component. */
@JNINamespace("browser_ui")
@NullMarked
public final class BottomSheetFeatureMap extends FeatureMap {
    public static final String BOTTOM_SHEET_TYPES = "BottomSheetTypes";

    private static final BottomSheetFeatureMap sInstance = new BottomSheetFeatureMap();

    public static final MutableFlagWithSafeDefault sBottomSheetTypes =
            newMutableFlagWithSafeDefault(BOTTOM_SHEET_TYPES, false);

    // Do not instantiate this class.
    private BottomSheetFeatureMap() {}

    /**
     * @return the singleton BottomSheetFeatureMap.
     */
    public static BottomSheetFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return BottomSheetFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return BottomSheetFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }
}
