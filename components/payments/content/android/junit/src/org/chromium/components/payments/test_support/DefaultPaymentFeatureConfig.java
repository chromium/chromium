// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.test_support;

import org.chromium.base.FeatureOverrides;
import org.chromium.components.payments.PaymentFeatureList;

/** Default flag configuration for payments features in unit tests. */
public abstract class DefaultPaymentFeatureConfig {
    /**
     * Sets the default flag configuration for payments feature flags for unit tests. Does not
     * override @EnableFeatures and @DisableFeatures annotations.
     */
    public static void setDefaultFlagConfigurationForTesting() {
        FeatureOverrides.newBuilder()
                .enable(PaymentFeatureList.WEB_PAYMENTS)
                .enable(PaymentFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP)
                .enable(PaymentFeatureList.WEB_PAYMENTS_EXPERIMENTAL_FEATURES)
                .disable(PaymentFeatureList.OMIT_PARAMETERS_IN_READY_TO_PAY)
                .applyWithoutOverwrite();
    }
}
