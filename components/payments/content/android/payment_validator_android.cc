// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/android/jni_headers/PaymentValidator_jni.h"
#include "components/payments/content/payment_request_converter.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_details_validation.h"
#include "components/payments/core/payments_validators.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

jboolean JNI_PaymentValidator_ValidatePaymentDetailsAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& buffer) {
  mojom::PaymentDetailsPtr details;
  if (!mojom::PaymentDetails::Deserialize(
          std::move(android::JavaByteBufferToNativeByteVector(env, buffer)),
          &details)) {
    return false;
  }
  std::string unused_error_message;
  return ValidatePaymentDetails(ConvertPaymentDetails(details),
                                &unused_error_message);
}

jboolean JNI_PaymentValidator_ValidatePaymentValidationErrorsAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& buffer) {
  mojom::PaymentValidationErrorsPtr errors;
  if (!mojom::PaymentValidationErrors::Deserialize(
          std::move(android::JavaByteBufferToNativeByteVector(env, buffer)),
          &errors)) {
    return false;
  }
  std::string unused_error_message;
  return PaymentsValidators::IsValidPaymentValidationErrorsFormat(
      std::move(errors), &unused_error_message);
}

}  // namespace payments
