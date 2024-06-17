// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/facilitated_payments_api_client_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/facilitated_payments/android/java/jni_headers/FacilitatedPaymentsApiClientBridge_jni.h"

namespace payments::facilitated {

std::unique_ptr<FacilitatedPaymentsApiClient>
LazyInitFacilitatedPaymentsApiClient(
    content::GlobalRenderFrameHostId render_frame_host_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id);
  return render_frame_host
             ? std::make_unique<FacilitatedPaymentsApiClientAndroid>(
                   render_frame_host)
             : nullptr;
}

// Declared in the cross-platform header
// `facilitated_payments_api_client_factory.h`.
FacilitatedPaymentsApiClientCreator GetFacilitatedPaymentsApiClientCreator(
    content::GlobalRenderFrameHostId render_frame_host_id) {
  return base::BindOnce(&LazyInitFacilitatedPaymentsApiClient,
                        render_frame_host_id);
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
    base::OnceCallback<void(PurchaseActionResult)> callback) {
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

void FacilitatedPaymentsApiClientAndroid::OnPurchaseActionResultEnum(
    JNIEnv* env,
    jint purchase_action_result) {
  if (!purchase_action_callback_ ||
      purchase_action_result <
          static_cast<int>(PurchaseActionResult::kCouldNotInvoke) ||
      purchase_action_result >
          static_cast<int>(PurchaseActionResult::kResultCanceled)) {
    return;
  }

  std::move(purchase_action_callback_)
      .Run(static_cast<PurchaseActionResult>(purchase_action_result));
}

bool FacilitatedPaymentsApiClientAndroid::IsAnyCallbackPending() const {
  return !is_available_callback_.is_null() ||
         !get_client_token_callback_.is_null() ||
         !purchase_action_callback_.is_null();
}

}  // namespace payments::facilitated
