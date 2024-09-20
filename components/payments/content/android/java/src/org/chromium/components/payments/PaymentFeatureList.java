// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.JNINamespace;

/**
 * Exposes payment specific features to java since files in org.chromium.components.payments cannot
 * depend on org.chromium.chrome.browser.flags.ChromeFeatureList.
 *
 * <p>Features listed here should be also registered in kFeaturesExposedToJava in
 * components/payments/content/android/payment_feature_map.cc
 */
@JNINamespace("payments::android")
public abstract class PaymentFeatureList {
    /** Alphabetical: */
    public static final String ENFORCE_FULL_DELEGATION = "EnforceFullDelegation";
    public static final String GPAY_APP_DYNAMIC_UPDATE = "GPayAppDynamicUpdate";
    public static final String OMIT_PARAMETERS_IN_READY_TO_PAY = "OmitParametersInReadyToPay";
    public static final String SERVICE_WORKER_PAYMENT_APPS = "ServiceWorkerPaymentApps";
    public static final String SHOW_READY_TO_PAY_DEBUG_INFO = "ShowReadyToPayDebugInfo";
    public static final String WEB_PAYMENTS = "WebPayments";
    public static final String WEB_PAYMENTS_APP_STORE_BILLING = "AppStoreBilling";
    public static final String WEB_PAYMENTS_APP_STORE_BILLING_DEBUG = "AppStoreBillingDebug";
    public static final String WEB_PAYMENTS_EXPERIMENTAL_FEATURES =
            "WebPaymentsExperimentalFeatures";
    public static final String WEB_PAYMENTS_SINGLE_APP_UI_SKIP = "WebPaymentsSingleAppUiSkip";

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in components/payments/content/android/payment_feature_map.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return PaymentFeatureMap.isEnabled(featureName);
    }

    /**
     * Returns whether the feature is enabled or not. Note: Features queried through this API must
     * be added to the array |kFeaturesExposedToJava| in
     * components/payments/content/android/payment_feature_map.cc
     *
     * @param featureName The name of the feature to query.
     * @return true when either the specified feature or |WEB_PAYMENTS_EXPERIMENTAL_FEATURES| is
     *     enabled.
     */
    public static boolean isEnabledOrExperimentalFeaturesEnabled(String featureName) {
        return isEnabled(WEB_PAYMENTS_EXPERIMENTAL_FEATURES) || isEnabled(featureName);
    }
}
