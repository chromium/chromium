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
public class ExternalIntentsFeatures extends Features {
    public static final String EXTERNAL_NAVIGATION_DEBUG_LOGS_NAME = "ExternalNavigationDebugLogs";
    public static final String BLOCK_INTENTS_TO_SELF_NAME = "BlockIntentsToSelf";
    public static final String NAVIGATION_CAPTURE_REFACTOR_ANDROID_NAME =
            "NavigationCaptureRefactorAndroid";

    public static final ExternalIntentsFeatures EXTERNAL_NAVIGATION_DEBUG_LOGS =
            new ExternalIntentsFeatures(0, EXTERNAL_NAVIGATION_DEBUG_LOGS_NAME);

    public static final ExternalIntentsFeatures BLOCK_INTENTS_TO_SELF =
            new ExternalIntentsFeatures(1, BLOCK_INTENTS_TO_SELF_NAME);

    public static final ExternalIntentsFeatures NAVIGATION_CAPTURE_REFACTOR_ANDROID =
            new ExternalIntentsFeatures(2, NAVIGATION_CAPTURE_REFACTOR_ANDROID_NAME);

    private final int mOrdinal;

    private ExternalIntentsFeatures(int ordinal, String name) {
        super(name);
        mOrdinal = ordinal;
    }

    @Override
    protected long getFeaturePointer() {
        return ExternalIntentsFeaturesJni.get().getFeature(mOrdinal);
    }

    @NativeMethods
    interface Natives {
        long getFeature(int ordinal);
    }
}
