// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_feature_list.h"

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/payments/content/android/jni_headers/PaymentFeatureList_jni.h"
#include "components/payments/core/features.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features_generated.h"

namespace payments {
namespace android {
namespace {

// Array of payment features exposed through the Java PaymentFeatureList API.
// Entries in this array refer to features defined in
// components/payments/core/features.h,
// content/public/common/content_features.h,
// third_party/blink/public/common/features_generated.h, or the .h file (for
// Android only features).
const base::Feature* const kFeaturesExposedToJava[] = {
    &::blink::features::kAddIdentityInCanMakePaymentEvent,
    &::blink::features::
        kAllowDiscoverableCredentialsForSecurePaymentConfirmation,
    &::features::kSecurePaymentConfirmation,
    &::features::kServiceWorkerPaymentApps,
    &::features::kWebPayments,
    &features::kAppStoreBilling,
    &features::kAppStoreBillingDebug,
    &features::kEnforceFullDelegation,
    &features::kGPayAppDynamicUpdate,
    &features::kWebPaymentsExperimentalFeatures,
    &features::kWebPaymentsModifiers,
    &features::kWebPaymentsSingleAppUiSkip,
    &kAndroidAppPaymentUpdateEvents,
    &kOmitParametersInReadyToPay,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const base::Feature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name) {
      return feature;
    }
  }
  NOTREACHED() << "Queried feature cannot be found in PaymentsFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

// Android only features.
BASE_FEATURE(kAndroidAppPaymentUpdateEvents,
             "AndroidAppPaymentUpdateEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOmitParametersInReadyToPay,
             "OmitParametersInReadyToPay",
             base::FEATURE_DISABLED_BY_DEFAULT);

static jboolean JNI_PaymentFeatureList_IsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature = FindFeatureExposedToJava(
      base::android::ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace android
}  // namespace payments
