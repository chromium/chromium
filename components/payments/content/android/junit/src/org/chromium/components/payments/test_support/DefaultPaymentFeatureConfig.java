// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.test_support;

import com.google.common.collect.ImmutableMap;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.components.payments.PaymentFeatureList;

import java.util.Map;

/** Default flag configuration for payments features in unit tests. */
public abstract class DefaultPaymentFeatureConfig {
    private static final Map<String, Boolean> DEFAULT_FEATURE_VALUES =
            ImmutableMap.<String, Boolean>builder()
                    .put(PaymentFeatureList.WEB_PAYMENTS, true)
                    .put(PaymentFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP, true)
                    .put(PaymentFeatureList.GPAY_APP_DYNAMIC_UPDATE, true)
                    .put(PaymentFeatureList.WEB_PAYMENTS_EXPERIMENTAL_FEATURES, true)
                    .put(PaymentFeatureList.OMIT_PARAMETERS_IN_READY_TO_PAY, false)
                    .buildOrThrow();

    /**
     * Sets the default flag configuration for payments feature flags for unit tests. Does not
     * override @EnableFeatures and @DisableFeatures annotations.
     */
    public static void setDefaultFlagConfigurationForTesting() {
        TestValues paymentsFeaturesOverrides = new TestValues();
        paymentsFeaturesOverrides.setFeatureFlagsOverride(DEFAULT_FEATURE_VALUES);
        FeatureList.mergeTestValues(paymentsFeaturesOverrides, false);
    }
}
