// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for base::Features listed in {@link NotificationsFeatureList} */
@JNINamespace("browser_ui")
public final class NotificationsFeatureMap extends FeatureMap {
    private static final NotificationsFeatureMap sInstance = new NotificationsFeatureMap();

    // Do not instantiate this class.
    private NotificationsFeatureMap() {}

    /**
     * @return the singleton NotificationsFeatureMap.
     */
    public static NotificationsFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return NotificationsFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
