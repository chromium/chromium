// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_feature_list.h"

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/payments/content/android/jni_headers/PaymentFeatureList_jni.h"
#include "components/payments/core/features.h"
#include "content/public/common/content_features.h"

namespace payments {
namespace android {
namespace {

// Array of payment features exposed through the Java PaymentFeatureList API.
// Entries in this array refer to features defined in
// components/payments/core/features.h, content/public/common/content_features.h
// or the .h file (for Android only features).
const base::Feature* kFeaturesExposedToJava[] = {
    &::features::kServiceWorkerPaymentApps,
    &::features::kWebPayments,
    &::features::kWebPaymentsMinimalUI,
    &features::kAlwaysAllowJustInTimePaymentApp,
    &features::kAppStoreBilling,
    &features::kAppStoreBillingDebug,
    &features::kEnforceFullDelegation,
    &features::kPaymentRequestSkipToGPay,
    &features::kPaymentRequestSkipToGPayIfNoCard,
    &features::kReturnGooglePayInBasicCard,
    &features::kStrictHasEnrolledAutofillInstrument,
    &features::kWebPaymentsExperimentalFeatures,
    &features::kWebPaymentsMethodSectionOrderV2,
    &features::kWebPaymentsModifiers,
    &features::kWebPaymentsRedactShippingAddress,
    &features::kWebPaymentsSingleAppUiSkip,
    &kAndroidAppPaymentUpdateEvents,
    &kScrollToExpandPaymentHandler,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (size_t i = 0; i < base::size(kFeaturesExposedToJava); ++i) {
    if (kFeaturesExposedToJava[i]->name == feature_name)
      return kFeaturesExposedToJava[i];
  }
  NOTREACHED() << "Queried feature cannot be found in PaymentsFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

// Android only features.
const base::Feature kAndroidAppPaymentUpdateEvents{
    "AndroidAppPaymentUpdateEvents", base::FEATURE_ENABLED_BY_DEFAULT};
// TODO(crbug.com/1094549): clean up after being stable.
const base::Feature kScrollToExpandPaymentHandler{
    "ScrollToExpandPaymentHandler", base::FEATURE_ENABLED_BY_DEFAULT};

static jboolean JNI_PaymentFeatureList_IsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature = FindFeatureExposedToJava(
      base::android::ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace android
}  // namespace payments
