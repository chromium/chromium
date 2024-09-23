// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/payment_request_converter.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_details_validation.h"
#include "components/payments/core/payments_validators.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/PaymentValidator_jni.h"

namespace payments {

jboolean JNI_PaymentValidator_ValidatePaymentDetailsAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& buffer) {
  mojom::PaymentDetailsPtr details;
  auto span = base::android::JavaByteBufferToSpan(env, buffer.obj());
  if (!mojom::PaymentDetails::Deserialize(span.data(), span.size(), &details)) {
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
  auto span = base::android::JavaByteBufferToSpan(env, buffer.obj());
  if (!mojom::PaymentValidationErrors::Deserialize(span.data(), span.size(),
                                                   &errors)) {
    return false;
  }
  std::string unused_error_message;
  return PaymentsValidators::IsValidPaymentValidationErrorsFormat(
      std::move(errors), &unused_error_message);
}

}  // namespace payments
