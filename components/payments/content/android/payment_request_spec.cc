// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_request_spec.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/minimal_jni/PaymentRequestSpec_jni.h"

namespace payments {
namespace android {

// static
jlong JNI_PaymentRequestSpec_Create(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& joptions_byte_buffer,
    const base::android::JavaParamRef<jobject>& jdetails_byte_buffer,
    const base::android::JavaParamRef<jobjectArray>& jmethod_data_byte_buffers,
    const base::android::JavaParamRef<jstring>& japp_locale) {
  mojom::PaymentOptionsPtr options;
  bool success =
      DeserializeFromJavaByteBuffer(env, joptions_byte_buffer, &options);
  DCHECK(success);

  mojom::PaymentDetailsPtr details;
  success = DeserializeFromJavaByteBuffer(env, jdetails_byte_buffer, &details);
  DCHECK(success);

  std::vector<mojom::PaymentMethodDataPtr> method_data;
  success = DeserializeFromJavaByteBufferArray(env, jmethod_data_byte_buffers,
                                               &method_data);
  DCHECK(success);

  return reinterpret_cast<intptr_t>(
      new PaymentRequestSpec(std::make_unique<payments::PaymentRequestSpec>(
          std::move(options), std::move(details), std::move(method_data),
          /*delegate=*/nullptr,
          base::android::ConvertJavaStringToUTF8(env, japp_locale))));
}

// static
base::WeakPtr<payments::PaymentRequestSpec>
PaymentRequestSpec::FromJavaPaymentRequestSpec(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jpayment_request_spec) {
  return reinterpret_cast<PaymentRequestSpec*>(
             Java_PaymentRequestSpec_getNativePointer(env,
                                                      jpayment_request_spec))
      ->spec_->AsWeakPtr();
}

PaymentRequestSpec::PaymentRequestSpec(
    std::unique_ptr<payments::PaymentRequestSpec> spec)
    : spec_(std::move(spec)) {}

void PaymentRequestSpec::UpdateWith(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jdetails_byte_buffer) {
  mojom::PaymentDetailsPtr details;
  bool success =
      DeserializeFromJavaByteBuffer(env, jdetails_byte_buffer, &details);
  DCHECK(success);

  spec_->UpdateWith(std::move(details));
}

void PaymentRequestSpec::Retry(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jvalidation_errors_buffer) {
  mojom::PaymentValidationErrorsPtr validation_errors;
  bool success = DeserializeFromJavaByteBuffer(env, jvalidation_errors_buffer,
                                               &validation_errors);
  DCHECK(success);

  spec_->Retry(std::move(validation_errors));
}

void PaymentRequestSpec::RecomputeSpecForDetails(JNIEnv* env) {
  spec_->RecomputeSpecForDetails();
}

bool PaymentRequestSpec::IsSecurePaymentConfirmationRequested(JNIEnv* env) {
  return spec_->IsSecurePaymentConfirmationRequested();
}

base::android::ScopedJavaLocalRef<jstring>
PaymentRequestSpec::SelectedShippingOptionError(JNIEnv* env) {
  return base::android::ConvertUTF16ToJavaString(
      env, spec_->selected_shipping_option_error());
}

base::android::ScopedJavaLocalRef<jbyteArray>
PaymentRequestSpec::GetPaymentDetails(JNIEnv* env) {
  return base::android::ToJavaByteArray(
      env, mojom::PaymentDetails::Serialize(&spec_->details_ptr()));
}

base::android::ScopedJavaLocalRef<jbyteArray>
PaymentRequestSpec::GetPaymentOptions(JNIEnv* env) {
  return base::android::ToJavaByteArray(
      env, mojom::PaymentOptions::Serialize(&spec_->payment_options()));
}

base::android::ScopedJavaLocalRef<jobjectArray>
PaymentRequestSpec::GetMethodData(JNIEnv* env) {
  return SerializeToJavaArrayOfByteArrays(env, spec_->method_data());
}

void PaymentRequestSpec::Destroy(JNIEnv* env) {
  delete this;
}

PaymentRequestSpec::~PaymentRequestSpec() = default;

}  // namespace android
}  // namespace payments
