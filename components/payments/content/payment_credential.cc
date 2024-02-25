// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/secure_payment_confirmation_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace payments {

PaymentCredential::PaymentCredential(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::PaymentCredential> receiver,
    scoped_refptr<PaymentManifestWebDataService> web_data_service)
    : DocumentService(render_frame_host, std::move(receiver)),
      web_data_service_(web_data_service) {}

PaymentCredential::~PaymentCredential() {
  Reset();
}

void PaymentCredential::StorePaymentCredential(
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    StorePaymentCredentialCallback callback) {
  if (state_ != State::kIdle || !IsCurrentStateValid() ||
      credential_id.empty() || rp_id.empty() || user_id.empty()) {
    Reset();
    std::move(callback).Run(
        mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_CREDENTIAL);
    return;
  }

  RecordFirstSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted);

  // If credential-store level APIs are available, the credential information
  // will already have been stored during creation.
  if (base::FeatureList::IsEnabled(
          features::kSecurePaymentConfirmationUseCredentialStoreAPIs)) {
    Reset();
    std::move(callback).Run(mojom::PaymentCredentialStorageStatus::SUCCESS);
    return;
  }

  storage_callback_ = std::move(callback);
  state_ = State::kStoringCredential;
  data_service_request_handle_ =
      web_data_service_->AddSecurePaymentConfirmationCredential(
          std::make_unique<SecurePaymentConfirmationCredential>(credential_id,
                                                                rp_id, user_id),
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
      result && static_cast<WDResult<bool>*>(result.get())->GetValue()
          ? mojom::PaymentCredentialStorageStatus::SUCCESS
          : mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_CREDENTIAL);
}

bool PaymentCredential::IsCurrentStateValid() const {
  if (!content::IsFrameAllowedToUseSecurePaymentConfirmation(
          &render_frame_host()) ||
      !web_data_service_) {
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
  if (storage_callback_) {
    std::move(storage_callback_)
        .Run(mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_CREDENTIAL);
  }

  if (web_data_service_ && data_service_request_handle_) {
    web_data_service_->CancelRequest(data_service_request_handle_.value());
  }

  data_service_request_handle_.reset();
  is_system_prompt_result_recorded_ = false;
  state_ = State::kIdle;
}

}  // namespace payments
