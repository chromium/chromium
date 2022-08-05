// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.os.Build;

import org.chromium.base.Features;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.BuildConfig;

/**
 * Java accessor for base/feature_list.h state.
 *
 * This class provides methods to access values of feature flags registered in
 * |kFeaturesExposedToJava| in components/external_intents/android/external_intents_features.cc.
 *
 */
@JNINamespace("external_intents")
public class ExternalIntentsFeatures extends Features {
    public static final String AUTOFILL_ASSISTANT_GOOGLE_INITIATOR_ORIGIN_CHECK_NAME =
            "AutofillAssistantGoogleInitiatorOriginCheck";
    public static final String SCARY_EXTERNAL_NAVIGATION_REFACTORING_NAME =
            "ScaryExternalNavigationRefactoring";

    public static final ExternalIntentsFeatures AUTOFILL_ASSISTANT_GOOGLE_INITIATOR_ORIGIN_CHECK =
            new ExternalIntentsFeatures(0, AUTOFILL_ASSISTANT_GOOGLE_INITIATOR_ORIGIN_CHECK_NAME);

    public static final ExternalIntentsFeatures SCARY_EXTERNAL_NAVIGATION_REFACTORING =
            new ExternalIntentsFeatures(1, SCARY_EXTERNAL_NAVIGATION_REFACTORING_NAME);

    private final int mOrdinal;

    private ExternalIntentsFeatures(int ordinal, String name) {
        super(name);
        mOrdinal = ordinal;
    }

    @Override
    public boolean isEnabled() {
        // Test-only hack to make sure we get coverage of the feature both enabled and disabled
        // across all test suites. CQ will run with the feature enabled on M, waterfall will run
        // with the feature disabled on various API levels.
        if (BuildConfig.IS_FOR_TEST && mOrdinal == SCARY_EXTERNAL_NAVIGATION_REFACTORING.mOrdinal) {
            return Build.VERSION.SDK_INT <= Build.VERSION_CODES.N;
        }
        return super.isEnabled();
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
