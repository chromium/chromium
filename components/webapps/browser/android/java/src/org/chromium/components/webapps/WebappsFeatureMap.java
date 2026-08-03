// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;

/** Java accessor for base::Features listed in webapps_feature_map.cc */
@JNINamespace("webapps")
@NullMarked
public class WebappsFeatureMap extends FeatureMap {
    public static final String ALWAYS_SHOW_INSTALL_DISAMBIGUATION_DIALOG =
            "AlwaysShowInstallDisambiguationDialog";

    private static final WebappsFeatureMap sInstance = new WebappsFeatureMap();

    // Do not instantiate this class.
    private WebappsFeatureMap() {}

    /** Returns the singleton WebappsFeatureMap. */
    public static WebappsFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return WebappsFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
