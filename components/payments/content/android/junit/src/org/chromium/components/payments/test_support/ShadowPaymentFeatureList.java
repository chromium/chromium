// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.test_support;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.components.payments.PaymentFeatureList;

import java.util.HashMap;
import java.util.Map;

/**
 * The shadow of PaymentFeatureList that allows unit tests to avoid loading native libraries to
 * check payment feature flag states. Usage example:
 *
 *   @RunWith(BaseRobolectricTestRunner.class)
 *   @Config(manifest = Config.NONE, shadows = {ShadowPaymentFeatureList.class})
 *   public class MyTest {}
 */
@Implements(PaymentFeatureList.class)
public class ShadowPaymentFeatureList {
    private static final Map<String, Boolean> sFeatureStatuses = new HashMap<>();

    public static void setDefaultStatuses() {
        ShadowPaymentFeatureList.setFeatureEnabled(PaymentFeatureList.WEB_PAYMENTS, true);
        ShadowPaymentFeatureList.setFeatureEnabled(
                PaymentFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP, true);
        ShadowPaymentFeatureList.setFeatureEnabled(
                PaymentFeatureList.GPAY_APP_DYNAMIC_UPDATE, true);
        ShadowPaymentFeatureList.setFeatureEnabled(
                PaymentFeatureList.WEB_PAYMENTS_EXPERIMENTAL_FEATURES, true);
        ShadowPaymentFeatureList.setFeatureEnabled(
                PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION, true);
        ShadowPaymentFeatureList.setFeatureEnabled(
                PaymentFeatureList.CLEAR_IDENTITY_IN_CAN_MAKE_PAYMENT_EVENT, false);
    }

    @Resetter
    public static void reset() {
        sFeatureStatuses.clear();
        setDefaultStatuses();
    }

    @Implementation
    public static boolean isEnabled(String featureName) {
        assert sFeatureStatuses.containsKey(featureName) : "The feature state has yet been set.";
        return sFeatureStatuses.get(featureName);
    }

    /**
     * Set the given feature to be enabled.
     * @param featureName The name of the feature.
     * @param enabled Whether to enable the feature.
     */
    public static void setFeatureEnabled(String featureName, boolean enabled) {
        sFeatureStatuses.put(featureName, enabled);
    }
}
