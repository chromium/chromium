// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/facilitated_payments_api_client_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/check.h"
#include "components/facilitated_payments/android/java/jni_headers/FacilitatedPaymentsApiClientBridge_jni.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/render_frame_host.h"

namespace payments::facilitated {

// Declared in the cross-platform header
// `facilitated_payments_api_client_factory.h`.
std::unique_ptr<FacilitatedPaymentsApiClient>
CreateFacilitatedPaymentsApiClient(
    content::RenderFrameHost* render_frame_host) {
  return std::make_unique<FacilitatedPaymentsApiClientAndroid>(
      render_frame_host);
}

FacilitatedPaymentsApiClientAndroid::FacilitatedPaymentsApiClientAndroid(
    content::RenderFrameHost* render_frame_host)
    : java_bridge_(Java_FacilitatedPaymentsApiClientBridge_Constructor(
          base::android::AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this),
          render_frame_host->GetJavaRenderFrameHost())) {}

FacilitatedPaymentsApiClientAndroid::~FacilitatedPaymentsApiClientAndroid() {
  Java_FacilitatedPaymentsApiClientBridge_resetNativePointer(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::IsAvailable(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(!IsAnyCallbackPending());

  is_available_callback_ = std::move(callback);
  Java_FacilitatedPaymentsApiClientBridge_isAvailable(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::GetClientToken(
    base::OnceCallback<void(std::vector<uint8_t>)> callback) {
  DCHECK(!IsAnyCallbackPending());

  get_client_token_callback_ = std::move(callback);
  Java_FacilitatedPaymentsApiClientBridge_getClientToken(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::InvokePurchaseAction(
    CoreAccountInfo primary_account,
    base::span<const uint8_t> action_token,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(!IsAnyCallbackPending());

  purchase_action_callback_ = std::move(callback);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FacilitatedPaymentsApiClientBridge_invokePurchaseAction(
      env, java_bridge_, ConvertToJavaCoreAccountInfo(env, primary_account),
      base::android::ToJavaByteArray(env, action_token));
}

void FacilitatedPaymentsApiClientAndroid::OnIsAvailable(
    JNIEnv* env,
    jboolean is_api_available) {
  if (is_available_callback_) {
    std::move(is_available_callback_).Run(is_api_available);
  }
}

void FacilitatedPaymentsApiClientAndroid::OnGetClientToken(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& jclient_token_byte_array) {
  if (get_client_token_callback_) {
    std::vector<uint8_t> client_token;
    if (jclient_token_byte_array) {
      base::android::JavaByteArrayToByteVector(env, jclient_token_byte_array,
                                               &client_token);
    }
    std::move(get_client_token_callback_).Run(std::move(client_token));
  }
}

void FacilitatedPaymentsApiClientAndroid::OnPurchaseActionResult(
    JNIEnv* env,
    jboolean is_purchase_action_successful) {
  if (purchase_action_callback_) {
    std::move(purchase_action_callback_).Run(is_purchase_action_successful);
  }
}

bool FacilitatedPaymentsApiClientAndroid::IsAnyCallbackPending() const {
  return !is_available_callback_.is_null() ||
         !get_client_token_callback_.is_null() ||
         !purchase_action_callback_.is_null();
}

}  // namespace payments::facilitated
