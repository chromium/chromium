// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview.internal;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/** Java accessor for base::Features related to the thin_webview component. */
@JNINamespace("thin_webview::android")
@NullMarked
public final class ThinWebViewFeatureMap extends FeatureMap {
    public static final String THIN_WEBVIEW_RELEASE_SURFACE_TEXTURE_ON_DESTROY =
            "ThinWebViewReleaseSurfaceTextureOnDestroy";

    private static final ThinWebViewFeatureMap sInstance = new ThinWebViewFeatureMap();

    public static final MutableFlagWithSafeDefault sThinWebViewReleaseSurfaceTextureOnDestroy =
            newMutableFlagWithSafeDefault(THIN_WEBVIEW_RELEASE_SURFACE_TEXTURE_ON_DESTROY, true);

    // Do not instantiate this class.
    private ThinWebViewFeatureMap() {}

    /**
     * @return the singleton ThinWebViewFeatureMap.
     */
    public static ThinWebViewFeatureMap getInstance() {
        return sInstance;
    }

    /**
     * Convenience method to call {@link #isEnabledInNative(String)} statically.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return ThinWebViewFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return ThinWebViewFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }
}
