// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Features;
import org.chromium.build.annotations.NullMarked;

/**
 * Java accessor for base/feature_list.h state.
 *
 * <p>This class provides methods to access values of feature flags registered in
 * |kFeaturesExposedToJava| in components/embedder_support/android/util/features.cc.
 */
@JNINamespace("embedder_support::features")
@NullMarked
public class EmbedderSupportFeatures {
    public static final String ANDROID_CHROME_SCHEME_NAVIGATION_KILL_SWITCH_NAME =
            "AndroidChromeSchemeNavigationKillSwitch";

    public static final EmbedderSupportFeature ANDROID_CHROME_SCHEME_NAVIGATION_KILL_SWITCH =
            new EmbedderSupportFeature(0, ANDROID_CHROME_SCHEME_NAVIGATION_KILL_SWITCH_NAME);

    public static class EmbedderSupportFeature extends Features {
        private final int mOrdinal;

        private EmbedderSupportFeature(int ordinal, String name) {
            super(name);
            mOrdinal = ordinal;
        }

        @Override
        protected long getFeaturePointer() {
            return EmbedderSupportFeaturesJni.get().getFeature(mOrdinal);
        }
    }

    @NativeMethods
    public interface Natives {
        long getFeature(int ordinal);
    }
}
