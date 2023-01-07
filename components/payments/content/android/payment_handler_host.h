// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_HOST_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_HOST_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/android/payment_request_update_event_listener.h"
#include "components/payments/content/payment_handler_host.h"

namespace payments {
namespace android {

// The native bridge for Java to interact with the payment handler host.
// Object relationship diagram:
//
// ChromePaymentRequestService.java --- implements --->
// PaymentRequestUpdateEventListener
//       |        ^
//      owns      |________________________
//       |                                |
//       v                                |
// PaymentHandlerHost.java                |
//       |                                |
//      owns                              |
//       |                             listener
//       v                                |
// android/payment_handler_host.h         |
//       |        |                       |
//      owns      |                       |
//       |       owns                     |
//       |        |                       |
//       |        v                       |
//       |    android/payment_request_update_event_listener.h
//       |        ^        \ ---- implements ---> PaymentHandlerHost::Delegate
//       |        |
//       |     delegate
//       v        |
// payment_handler_host.h
class PaymentHandlerHost {
 public:
  // Converts a Java PaymentHandlerHost object into a C++ cross-platform
  // payments::PaymentHandlerHost object. The returned object is ultimately
  // owned by the Java PaymentHandlerHost.
  static base::WeakPtr<payments::PaymentHandlerHost> FromJavaPaymentHandlerHost(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& payment_handler_host);

  // The |listener| must implement PaymentRequestUpdateEventListener. The
  // |web_contents| should be from the same browser context as the payment
  // handler and are used for logging in developr tools.
  PaymentHandlerHost(const base::android::JavaParamRef<jobject>& web_contents,
                     const base::android::JavaParamRef<jobject>& listener);

  PaymentHandlerHost(const PaymentHandlerHost&) = delete;
  PaymentHandlerHost& operator=(const PaymentHandlerHost&) = delete;

  ~PaymentHandlerHost();

  // Checks whether any payment method, shipping address or shipping option
  // change is currently in progress.
  jboolean IsWaitingForPaymentDetailsUpdate(JNIEnv* env) const;

  // Destroys this object.
  void Destroy(JNIEnv* env);

  // Notifies the payment handler that the merchant has updated the payment
  // details. The |response_buffer| should be a serialization of a valid
  // PaymentRequestDetailsUpdate.java object.
  void UpdateWith(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& response_buffer);

  // Notifies the payment handler that the merchant ignored the payment
  // method change event.
  void OnPaymentDetailsNotUpdated(JNIEnv* env);

 private:
  PaymentRequestUpdateEventListener listener_;
  payments::PaymentHandlerHost payment_handler_host_;
};

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_HOST_H_
