// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_feature_map.h"

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/payments/core/features.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features_generated.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/feature_list_jni/PaymentFeatureMap_jni.h"

namespace payments::android {
namespace {

// Array of payment features exposed through the Java PaymentFeatureList API.
// Entries in this array refer to features defined in
// components/payments/core/features.h,
// content/public/common/content_features.h,
// third_party/blink/public/common/features_generated.h, or the .h file (for
// Android only features).
const base::Feature* const kFeaturesExposedToJava[] = {
    &::features::kServiceWorkerPaymentApps,
    &::features::kWebPayments,
    &features::kAppStoreBilling,
    &features::kAppStoreBillingDebug,
    &features::kEnforceFullDelegation,
    &features::kGPayAppDynamicUpdate,
    &features::kWebPaymentsExperimentalFeatures,
    &features::kWebPaymentsSingleAppUiSkip,
    &kOmitParametersInReadyToPay,
    &kShowReadyToPayDebugInfo,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_PaymentFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

// Android only features.
BASE_FEATURE(kOmitParametersInReadyToPay,
             "OmitParametersInReadyToPay",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShowReadyToPayDebugInfo,
             "ShowReadyToPayDebugInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace payments::android
