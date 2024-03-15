// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_

#include <jni.h>
#include <cstdint>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"

namespace payments::facilitated {

class FacilitatedPaymentsApiClientDelegate;

// Android implementation for facilitated payment APIs, such as PIX. Uses
// Android APIs through JNI.
class FacilitatedPaymentsApiClientAndroid
    : public FacilitatedPaymentsApiClient {
 public:
  explicit FacilitatedPaymentsApiClientAndroid(
      base::WeakPtr<FacilitatedPaymentsApiClientDelegate> delegate);
  ~FacilitatedPaymentsApiClientAndroid() override;

  FacilitatedPaymentsApiClientAndroid(
      const FacilitatedPaymentsApiClientAndroid& other) = delete;
  FacilitatedPaymentsApiClientAndroid& operator=(
      const FacilitatedPaymentsApiClientAndroid& other) = delete;

  // FacilitatedPaymentsApiClient implementation:
  void IsAvailable() override;
  void GetClientToken() override;
  void InvokePurchaseAction(base::span<const uint8_t> action_token) override;

  void OnIsAvailable(JNIEnv* env, jboolean is_available);
  void OnGetClientToken(
      JNIEnv* env,
      const base::android::JavaRef<jbyteArray>& jclient_token_byte_array);
  void OnPurchaseActionResult(JNIEnv* env,
                              jboolean is_purchase_action_successful);

 private:
  base::WeakPtr<FacilitatedPaymentsApiClientDelegate> delegate_;
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_
