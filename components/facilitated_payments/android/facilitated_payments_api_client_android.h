// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_

#include <jni.h>
#include <cstdint>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"

namespace payments::facilitated {

// Android implementation for facilitated payment APIs, such as PIX. Uses
// Android APIs through JNI.
class FacilitatedPaymentsApiClientAndroid {
 public:
  FacilitatedPaymentsApiClientAndroid();
  virtual ~FacilitatedPaymentsApiClientAndroid();

  FacilitatedPaymentsApiClientAndroid(
      const FacilitatedPaymentsApiClientAndroid& other) = delete;
  FacilitatedPaymentsApiClientAndroid& operator=(
      const FacilitatedPaymentsApiClientAndroid& other) = delete;

  // Checks whether the facilitated payment API is available. The response is
  // received in the OnIsAvailable() method. (If the API is not available, there
  // is no need to show FOPs to the user.)
  void IsAvailable();

  // Retrieves the client token to be used to initiate a payment. The response
  // is received in the OnGetClientToken() method.
  void GetClientToken();

  // Invokes the purchase manager with the given action token. The result is
  // received in the OnPurchaseActionResult() method.
  void InvokePurchaseAction(base::span<const uint8_t> action_token);

  // Virtual because tests override these methods.
  virtual void OnIsAvailable(JNIEnv* env, jboolean is_available);
  virtual void OnGetClientToken(JNIEnv* env, jobject jclient_token_byte_array);
  virtual void OnPurchaseActionResult(JNIEnv* env,
                                      jboolean is_purchase_action_successful);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_
