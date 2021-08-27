// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace payments {

// static
bool PaymentCredential::IsFrameAllowedToUseSecurePaymentConfirmation(
    content::RenderFrameHost* rfh) {
  return rfh && rfh->IsActive() &&
         rfh->IsFeatureEnabled(
             blink::mojom::PermissionsPolicyFeature::kPayment) &&
         base::FeatureList::IsEnabled(features::kSecurePaymentConfirmation);
}

PaymentCredential::PaymentCredential(
    content::WebContents* web_contents,
    content::GlobalRenderFrameHostId initiator_frame_routing_id,
    scoped_refptr<PaymentManifestWebDataService> web_data_service,
    mojo::PendingReceiver<mojom::PaymentCredential> receiver)
    : WebContentsObserver(web_contents),
      initiator_frame_routing_id_(initiator_frame_routing_id),
      web_data_service_(web_data_service) {
  DCHECK(web_contents);
  receiver_.Bind(std::move(receiver));
}

PaymentCredential::~PaymentCredential() {
  Reset();
}

void PaymentCredential::StorePaymentCredential(
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    StorePaymentCredentialCallback callback) {
  if (state_ != State::kIdle || !IsCurrentStateValid() ||
      credential_id.empty() || rp_id.empty()) {
    Reset();
    std::move(callback).Run(
        mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_INSTRUMENT);
    return;
  }

  RecordFirstSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted);

  storage_callback_ = std::move(callback);
  state_ = State::kStoringCredential;
  data_service_request_handle_ =
      web_data_service_->AddSecurePaymentConfirmationInstrument(
          std::make_unique<SecurePaymentConfirmationInstrument>(credential_id,
                                                                rp_id),
          /*consumer=*/this);
}

void PaymentCredential::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  if (state_ != State::kStoringCredential || !IsCurrentStateValid() ||
      data_service_request_handle_ != h) {
    Reset();
    return;
  }

  auto callback = std::move(storage_callback_);
  Reset();

  std::move(callback).Run(
      static_cast<WDResult<bool>*>(result.get())->GetValue()
          ? mojom::PaymentCredentialStorageStatus::SUCCESS
          : mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_INSTRUMENT);
}

void PaymentCredential::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Reset the service before the page navigates away.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsSameDocument() &&
      (navigation_handle->IsInPrimaryMainFrame() ||
       navigation_handle->GetPreviousRenderFrameHostId() ==
           initiator_frame_routing_id_)) {
    Reset();
  }
}

void PaymentCredential::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // Reset the service before the render frame is deleted.
  if (render_frame_host == web_contents()->GetMainFrame() ||
      render_frame_host ==
          content::RenderFrameHost::FromID(initiator_frame_routing_id_)) {
    Reset();
  }
}

bool PaymentCredential::IsCurrentStateValid() const {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(initiator_frame_routing_id_);

  if (!IsFrameAllowedToUseSecurePaymentConfirmation(render_frame_host) ||
      !web_contents() ||
      web_contents() !=
          content::WebContents::FromRenderFrameHost(render_frame_host) ||
      !web_data_service_ || !receiver_.is_bound()) {
    return false;
  }

  switch (state_) {
    case State::kIdle:
      return !storage_callback_ && !data_service_request_handle_;

    case State::kStoringCredential:
      return storage_callback_ && data_service_request_handle_;
  }
}

void PaymentCredential::RecordFirstSystemPromptResult(
    SecurePaymentConfirmationEnrollSystemPromptResult result) {
  if (!is_system_prompt_result_recorded_) {
    is_system_prompt_result_recorded_ = true;
    RecordEnrollSystemPromptResult(result);
  }
}

void PaymentCredential::Reset() {
  // Callbacks must either be run or disconnected before being destroyed, so
  // run them if they are still connected.
  if (receiver_.is_bound()) {
    if (storage_callback_) {
      std::move(storage_callback_)
          .Run(mojom::PaymentCredentialStorageStatus::
                   FAILED_TO_STORE_INSTRUMENT);
    }
  }

  if (web_data_service_ && data_service_request_handle_) {
    web_data_service_->CancelRequest(data_service_request_handle_.value());
  }

  data_service_request_handle_.reset();
  is_system_prompt_result_recorded_ = false;
  state_ = State::kIdle;
}

}  // namespace payments
