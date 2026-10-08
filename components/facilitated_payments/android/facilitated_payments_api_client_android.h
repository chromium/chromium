// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_

#include <jni.h>

#include <cstdint>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace payments::facilitated {

// Android implementation for facilitated payment APIs, such as Pix. Uses
// Android APIs through JNI.
class FacilitatedPaymentsApiClientAndroid
    : public FacilitatedPaymentsApiClient {
 public:
  // Creates an instance of the facilitated payment API bridge. Uses the given
  // `render_frame_host` to retrieve the Android context. The
  // `render_frame_host` should not be null.
  explicit FacilitatedPaymentsApiClientAndroid(
      content::RenderFrameHost* render_frame_host);
  ~FacilitatedPaymentsApiClientAndroid() override;

  FacilitatedPaymentsApiClientAndroid(
      const FacilitatedPaymentsApiClientAndroid& other) = delete;
  FacilitatedPaymentsApiClientAndroid& operator=(
      const FacilitatedPaymentsApiClientAndroid& other) = delete;

  // FacilitatedPaymentsApiClient implementation:
  void IsAvailable(base::OnceCallback<void(bool)> callback) override;
  bool IsAvailableSync() override;
  void GetClientToken(
      base::OnceCallback<void(std::vector<uint8_t>)> callback) override;
  void InvokePurchaseAction(
      CoreAccountInfo primary_account,
      const SecurePayload& secure_payload,
      base::OnceCallback<void(PurchaseActionResult)> callback) override;
  void InvokeInstrumentManager(
      CoreAccountInfo primary_account,
      const std::vector<uint8_t>& action_token,
      base::OnceCallback<void(AccountLinkingResult)> callback) override;

  void OnIsAvailable(JNIEnv* env, bool is_available);
  void OnGetClientToken(
      JNIEnv* env,
      const base::android::JavaRef<jbyteArray>& jclient_token_byte_array);
  void OnPurchaseActionResultEnum(JNIEnv* env, int32_t purchase_action_result);
  void OnInvokeInstrumentManagerResult(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jaccount_linking_result);

  content::GlobalRenderFrameHostId GetRenderFrameHostIdForTesting() const {
    return render_frame_host_id_;
  }

 private:
  bool IsAnyCallbackPending() const;

  base::WeakPtr<content::WebContents> web_contents_;
  content::GlobalRenderFrameHostId render_frame_host_id_;
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
  base::OnceCallback<void(bool)> is_available_callback_;
  base::OnceCallback<void(std::vector<uint8_t>)> get_client_token_callback_;
  base::OnceCallback<void(PurchaseActionResult)> purchase_action_callback_;
  base::OnceCallback<void(AccountLinkingResult)>
      invoke_instrument_manager_callback_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_FACILITATED_PAYMENTS_API_CLIENT_ANDROID_H_
