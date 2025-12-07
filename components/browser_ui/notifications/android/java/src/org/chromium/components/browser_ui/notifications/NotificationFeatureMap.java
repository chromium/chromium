// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;

/** Java accessor for base::Features listed in native */
@JNINamespace("browser_ui")
@NullMarked
public final class NotificationFeatureMap extends FeatureMap {
    public static final String CACHE_NOTIIFICATIONS_ENABLED = "CacheNotificationsEnabled";

    private static final NotificationFeatureMap sInstance = new NotificationFeatureMap();

    // Do not instantiate this class.
    private NotificationFeatureMap() {}

    /**
     * @return the singleton NotificationFeatureMap.
     */
    public static NotificationFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return NotificationFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
