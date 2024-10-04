// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_request_update_event_listener.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/service_jni/PaymentRequestUpdateEventListener_jni.h"

namespace payments {
namespace android {

PaymentRequestUpdateEventListener::PaymentRequestUpdateEventListener(
    const base::android::JavaParamRef<jobject>& listener)
    : listener_(listener) {}

PaymentRequestUpdateEventListener::~PaymentRequestUpdateEventListener() =
    default;

base::WeakPtr<PaymentRequestUpdateEventListener>
PaymentRequestUpdateEventListener::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool PaymentRequestUpdateEventListener::ChangePaymentMethod(
    const std::string& method_name,
    const std::string& stringified_data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PaymentRequestUpdateEventListener_changePaymentMethodFromInvokedApp(
      env, listener_, base::android::ConvertUTF8ToJavaString(env, method_name),
      base::android::ConvertUTF8ToJavaString(env, stringified_data));
}

bool PaymentRequestUpdateEventListener::ChangeShippingOption(
    const std::string& shipping_option_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PaymentRequestUpdateEventListener_changeShippingOptionFromInvokedApp(
      env, listener_,
      base::android::ConvertUTF8ToJavaString(env, shipping_option_id));
}

bool PaymentRequestUpdateEventListener::ChangeShippingAddress(
    mojom::PaymentAddressPtr shipping_address) {
  std::vector<uint8_t> byte_vector =
      mojom::PaymentAddress::Serialize(&shipping_address);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj(
      env, env->NewDirectByteBuffer(byte_vector.data(), byte_vector.size()));
  base::android::CheckException(env);
  return Java_PaymentRequestUpdateEventListener_changeShippingAddress(
      env, listener_, obj);
}

}  // namespace android
}  // namespace payments
