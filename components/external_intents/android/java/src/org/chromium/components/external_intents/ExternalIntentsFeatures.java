// Copyright 2021 The Chromium Authors. All rights reserved.
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
 * |kFeaturesExposedToJava| in components/external_intents/android/external_intents_feature_list.cc.
 *
 */
@JNINamespace("external_intents")
public class ExternalIntentsFeatures extends Features {
    public static final String INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE_NAME =
            "IntentBlockExternalFormRedirectsNoGesture";

    public static final ExternalIntentsFeatures INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE =
            new ExternalIntentsFeatures(0, INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE_NAME);

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
