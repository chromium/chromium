// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/facilitated_payments_api_client_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "components/facilitated_payments/android/java/jni_headers/FacilitatedPaymentsApiClientBridge_jni.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client_delegate.h"

namespace payments::facilitated {

// Declared in the cross-platform header `facilitated_payments_api_client.h`.
// static
std::unique_ptr<FacilitatedPaymentsApiClient>
FacilitatedPaymentsApiClient::Create(
    base::WeakPtr<FacilitatedPaymentsApiClientDelegate> delegate) {
  return std::make_unique<FacilitatedPaymentsApiClientAndroid>(
      std::move(delegate));
}

FacilitatedPaymentsApiClientAndroid::FacilitatedPaymentsApiClientAndroid(
    base::WeakPtr<FacilitatedPaymentsApiClientDelegate> delegate)
    : delegate_(std::move(delegate)),
      java_bridge_(Java_FacilitatedPaymentsApiClientBridge_Constructor(
          base::android::AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this))) {}

FacilitatedPaymentsApiClientAndroid::~FacilitatedPaymentsApiClientAndroid() {
  Java_FacilitatedPaymentsApiClientBridge_resetNativePointer(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::IsAvailable() {
  Java_FacilitatedPaymentsApiClientBridge_isAvailable(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::GetClientToken() {
  Java_FacilitatedPaymentsApiClientBridge_getClientToken(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::InvokePurchaseAction(
    base::span<const uint8_t> action_token) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FacilitatedPaymentsApiClientBridge_invokePurchaseAction(
      env, java_bridge_, base::android::ToJavaByteArray(env, action_token));
}

void FacilitatedPaymentsApiClientAndroid::OnIsAvailable(
    JNIEnv* env,
    jboolean is_api_available) {
  if (delegate_) {
    delegate_->OnIsAvailable(is_api_available);
  }
}

void FacilitatedPaymentsApiClientAndroid::OnGetClientToken(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& jclient_token_byte_array) {
  if (delegate_) {
    std::vector<uint8_t> client_token;
    if (jclient_token_byte_array) {
      base::android::JavaByteArrayToByteVector(env, jclient_token_byte_array,
                                               &client_token);
    }
    delegate_->OnGetClientToken(std::move(client_token));
  }
}

void FacilitatedPaymentsApiClientAndroid::OnPurchaseActionResult(
    JNIEnv* env,
    jboolean is_purchase_action_successful) {
  if (delegate_) {
    delegate_->OnPurchaseActionResult(is_purchase_action_successful);
  }
}

}  // namespace payments::facilitated
