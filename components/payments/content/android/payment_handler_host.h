// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_HOST_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_HOST_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/payments/content/payment_handler_host.h"

namespace payments {
namespace android {

// The native bridge for Java to interact with the payment handler host.
// Object relationship diagram:
//
// PaymentRequestImpl.java ---- implements ----> PaymentHandlerHostDelegate
//       |        ^
//      owns      |_________
//       |                  |
//       v                  |
// PaymentHandlerHost.java  |
//       |                  |
//      owns                |
//       |               delegate
//       v                  |
// android/payment_handler_host.h -- implements -> PaymentHandlerHost::Delegate
//       |        ^
//      owns      |
//       |     delegate
//       v        |
// payment_handler_host.h
class PaymentHandlerHost : public payments::PaymentHandlerHost::Delegate {
 public:
  // The |delegate| must implement PaymentHandlerHostDelegate from
  // PaymentHandlerHost.java. The |web_contents| should be from the same browser
  // context as the payment handler and are used for logging in developr tools.
  PaymentHandlerHost(const base::android::JavaParamRef<jobject>& web_contents,
                     const base::android::JavaParamRef<jobject>& delegate);
  ~PaymentHandlerHost() override;

  // Checks whether the payment method change is currently in progress.
  jboolean IsChangingPaymentMethod(JNIEnv* env) const;

  // Returns the pointer to the payments::PaymentHandlerHost for binding to its
  // IPC endpoint in service_worker_payment_app_bridge.cc.
  jlong GetNativePaymentHandlerHost(JNIEnv* env);

  // Destroys this object.
  void Destroy(JNIEnv* env);

  // Notifies the payment handler that the merchant has updated the payment
  // details. The |response_buffer| should be a serialization of a valid
  // PaymentRequestDetailsUpdate.java object.
  void UpdateWith(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& response_buffer);

  // Notifies the payment handler that the merchant ignored the payment
  // method change event.
  void NoUpdatedPaymentDetails(JNIEnv* env);

 private:
  // PaymentHandlerHost::Delegate implementation:
  bool ChangePaymentMethod(const std::string& method_name,
                           const std::string& stringified_data) override;
  bool ChangeShippingOption(const std::string& shipping_option_id) override;
  bool ChangeShippingAddress(
      mojom::PaymentAddressPtr shipping_address) override;

  base::android::ScopedJavaGlobalRef<jobject> delegate_;
  payments::PaymentHandlerHost payment_handler_host_;

  DISALLOW_COPY_AND_ASSIGN(PaymentHandlerHost);
};

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_HOST_H_
