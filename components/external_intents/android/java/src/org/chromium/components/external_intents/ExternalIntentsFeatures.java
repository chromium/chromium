// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import org.chromium.base.Features;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for base/feature_list.h state.
 *
 * This class provides methods to access values of feature flags registered in
 * |kFeaturesExposedToJava| in components/external_intents/android/external_intents_features.cc.
 *
 */
@JNINamespace("external_intents")
public class ExternalIntentsFeatures extends Features {
    public static final String BLOCK_EXTERNAL_FORM_SUBMIT_WITHOUT_GESTURE_NAME =
            "BlockExternalFormSubmitWithoutGesture";
    public static final String EXTERNAL_NAVIGATION_DEBUG_LOGS_NAME = "ExternalNavigationDebugLogs";

    public static final ExternalIntentsFeatures BLOCK_EXTERNAL_FORM_SUBMIT_WITHOUT_GESTURE =
            new ExternalIntentsFeatures(0, BLOCK_EXTERNAL_FORM_SUBMIT_WITHOUT_GESTURE_NAME);

    public static final ExternalIntentsFeatures EXTERNAL_NAVIGATION_DEBUG_LOGS =
            new ExternalIntentsFeatures(1, EXTERNAL_NAVIGATION_DEBUG_LOGS_NAME);

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
