// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Features;
import org.chromium.build.annotations.NullMarked;

/**
 * Java accessor for base/feature_list.h state.
 *
 * <p>This class provides methods to access values of feature flags registered in
 * |kFeaturesExposedToJava| in components/external_intents/android/external_intents_features.cc.
 */
@JNINamespace("external_intents")
@NullMarked
public class ExternalIntentsFeatures {
    public static final String EXTERNAL_NAVIGATION_DEBUG_LOGS_NAME = "ExternalNavigationDebugLogs";
    public static final String NAVIGATION_CAPTURE_REFACTOR_ANDROID_NAME =
            "NavigationCaptureRefactorAndroid";

    public static final ExternalIntentsFeature EXTERNAL_NAVIGATION_DEBUG_LOGS =
            new ExternalIntentsFeature(0, EXTERNAL_NAVIGATION_DEBUG_LOGS_NAME);

    public static final ExternalIntentsFeature NAVIGATION_CAPTURE_REFACTOR_ANDROID =
            new ExternalIntentsFeature(1, NAVIGATION_CAPTURE_REFACTOR_ANDROID_NAME);

    public static class ExternalIntentsFeature extends Features {
        private final int mOrdinal;

        private ExternalIntentsFeature(int ordinal, String name) {
            super(name);
            mOrdinal = ordinal;
        }

        @Override
        protected long getFeaturePointer() {
            return ExternalIntentsFeaturesJni.get().getFeature(mOrdinal);
        }
    }

    @NativeMethods
    interface Natives {
        long getFeature(int ordinal);
    }
}
