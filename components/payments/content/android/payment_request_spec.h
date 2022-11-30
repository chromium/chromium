// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_REQUEST_SPEC_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_REQUEST_SPEC_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/payments/content/payment_request_spec.h"

namespace payments {
namespace android {

// A bridge for Android to own a C++ PaymentRequestSpec object.
//
// Object ownership diagram:
//
// ChromePaymentRequestService.java
//       |
//       v
// PaymentRequestSpec.java
//       |
//       v
// android/payment_request_spec.h
//       |
//       v
// payment_request_spec.h
class PaymentRequestSpec {
 public:
  // Returns the C++ PaymentRequestSpec that is owned by the Java
  // PaymentRequestSpec, or nullptr after the Java method
  // PaymentRequestSpec.destroy() has been called.
  static base::WeakPtr<payments::PaymentRequestSpec> FromJavaPaymentRequestSpec(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jpayment_request_spec);

  // Constructs the Android bridge with the given |spec|.
  explicit PaymentRequestSpec(
      std::unique_ptr<payments::PaymentRequestSpec> spec);

  // Called when the renderer updates the payment details in response to, e.g.,
  // new shipping address.
  void UpdateWith(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jdetails_buffer);

  // Called when the merchant retries a failed payment.
  void Retry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jvalidation_errors_buffer);

  // Recomputes spec based on details.
  void RecomputeSpecForDetails(JNIEnv* env);

  // Returns whether the secure-payment-confirmation method is requested.
  bool IsSecurePaymentConfirmationRequested(JNIEnv* env);

  // Returns the selected shipping option error.
  base::android::ScopedJavaLocalRef<jstring> SelectedShippingOptionError(
      JNIEnv* env);

  // Returns the payment details.
  base::android::ScopedJavaLocalRef<jbyteArray> GetPaymentDetails(JNIEnv* env);

  // Returns the payment options.
  base::android::ScopedJavaLocalRef<jbyteArray> GetPaymentOptions(JNIEnv* env);

  // Returns the method data.
  base::android::ScopedJavaLocalRef<jobjectArray> GetMethodData(JNIEnv* env);

  // Destroys this bridge.
  void Destroy(JNIEnv* env);

 private:
  ~PaymentRequestSpec();

  std::unique_ptr<payments::PaymentRequestSpec> spec_;
};

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_REQUEST_SPEC_H_
