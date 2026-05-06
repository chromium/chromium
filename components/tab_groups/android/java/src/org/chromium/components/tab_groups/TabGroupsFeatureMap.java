// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_groups;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/** Java accessor for base::Features listed in native */
@JNINamespace("tab_groups_android")
@NullMarked
public class TabGroupsFeatureMap extends FeatureMap {
    public static final String UPDATE_TAB_GROUP_COLORS = "UpdateTabGroupColors";

    private static final TabGroupsFeatureMap sInstance = new TabGroupsFeatureMap();

    public static final MutableFlagWithSafeDefault sUpdateTabGroupColors =
            newMutableFlagWithSafeDefault(UPDATE_TAB_GROUP_COLORS, false);

    // Do not instantiate this class.
    private TabGroupsFeatureMap() {}

    /** Returns the singleton TabGroupsFeatureMap. */
    public static TabGroupsFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return TabGroupsFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return TabGroupsFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }
}
