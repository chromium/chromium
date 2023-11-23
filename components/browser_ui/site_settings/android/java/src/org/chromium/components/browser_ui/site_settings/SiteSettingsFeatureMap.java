// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for base::Features listed in {@link SiteSettingsFeatureList} */
@JNINamespace("browser_ui")
public final class SiteSettingsFeatureMap extends FeatureMap {
    private static final SiteSettingsFeatureMap sInstance = new SiteSettingsFeatureMap();

    // Do not instantiate this class.
    private SiteSettingsFeatureMap() {}

    /** @return the singleton SiteSettingsFeatureMap. */
    public static SiteSettingsFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return SiteSettingsFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
