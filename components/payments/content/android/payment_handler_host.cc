// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_handler_host.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
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
    const base::android::JavaParamRef<jobject>& listener) {
  return reinterpret_cast<intptr_t>(
      new PaymentHandlerHost(web_contents, listener));
}

// static
base::WeakPtr<payments::PaymentHandlerHost>
PaymentHandlerHost::FromJavaPaymentHandlerHost(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& payment_handler_host) {
  return reinterpret_cast<PaymentHandlerHost*>(
             Java_PaymentHandlerHost_getNativeBridge(env, payment_handler_host))
      ->payment_handler_host_.AsWeakPtr();
}

PaymentHandlerHost::PaymentHandlerHost(
    const base::android::JavaParamRef<jobject>& web_contents,
    const base::android::JavaParamRef<jobject>& listener)
    : listener_(listener),
      payment_handler_host_(
          content::WebContents::FromJavaWebContents(web_contents),
          /*delegate=*/listener_.AsWeakPtr()) {}

PaymentHandlerHost::~PaymentHandlerHost() {}

jboolean PaymentHandlerHost::IsWaitingForPaymentDetailsUpdate(
    JNIEnv* env) const {
  return payment_handler_host_.is_waiting_for_payment_details_update();
}

void PaymentHandlerHost::Destroy(JNIEnv* env) {
  delete this;
}

void PaymentHandlerHost::UpdateWith(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& response_buffer) {
  mojom::PaymentRequestDetailsUpdatePtr response;
  bool success = mojom::PaymentRequestDetailsUpdate::Deserialize(
      JavaByteBufferToNativeByteVector(env, response_buffer), &response);
  DCHECK(success);
  payment_handler_host_.UpdateWith(std::move(response));
}

void PaymentHandlerHost::OnPaymentDetailsNotUpdated(JNIEnv* env) {
  payment_handler_host_.OnPaymentDetailsNotUpdated();
}

}  // namespace android
}  // namespace payments
