// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_handler_host.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/android/jni_headers/PaymentHandlerHost_jni.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"

namespace payments {
namespace android {

// static
jlong JNI_PaymentHandlerHost_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& web_contents,
    const base::android::JavaParamRef<jobject>& delegate) {
  return reinterpret_cast<intptr_t>(
      new PaymentHandlerHost(web_contents, delegate));
}

PaymentHandlerHost::PaymentHandlerHost(
    const base::android::JavaParamRef<jobject>& web_contents,
    const base::android::JavaParamRef<jobject>& delegate)
    : delegate_(delegate),
      payment_handler_host_(
          content::WebContents::FromJavaWebContents(web_contents),
          /*delegate=*/this) {}

PaymentHandlerHost::~PaymentHandlerHost() {}

jboolean PaymentHandlerHost::IsChangingPaymentMethod(JNIEnv* env) const {
  return payment_handler_host_.is_changing();
}

jlong PaymentHandlerHost::GetNativePaymentHandlerHost(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(&payment_handler_host_);
}

void PaymentHandlerHost::Destroy(JNIEnv* env) {
  delete this;
}

void PaymentHandlerHost::UpdateWith(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& response_buffer) {
  mojom::PaymentRequestDetailsUpdatePtr response;
  bool success = mojom::PaymentRequestDetailsUpdate::Deserialize(
      std::move(JavaByteBufferToNativeByteVector(env, response_buffer)),
      &response);
  DCHECK(success);
  payment_handler_host_.UpdateWith(std::move(response));
}

void PaymentHandlerHost::NoUpdatedPaymentDetails(JNIEnv* env) {
  payment_handler_host_.NoUpdatedPaymentDetails();
}

bool PaymentHandlerHost::ChangePaymentMethod(
    const std::string& method_name,
    const std::string& stringified_data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PaymentHandlerHostDelegate_changePaymentMethodFromPaymentHandler(
      env, delegate_, base::android::ConvertUTF8ToJavaString(env, method_name),
      base::android::ConvertUTF8ToJavaString(env, stringified_data));
}

bool PaymentHandlerHost::ChangeShippingOption(
    const std::string& shipping_option_id) {
  // Shipping and contact info delegation is not implemented on Android yet.
  // TODO(sahel): crbug.com/984694
  NOTREACHED();
  return false;
}

bool PaymentHandlerHost::ChangeShippingAddress(
    mojom::PaymentAddressPtr shipping_address) {
  // Shipping and contact info delegation is not implemented on Android yet.
  // TODO(sahel): crbug.com/984694
  NOTREACHED();
  return false;
}

}  // namespace android
}  // namespace payments
