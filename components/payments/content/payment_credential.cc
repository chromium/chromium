// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/payments/core/url_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-shared.h"
#include "ui/gfx/image/image.h"

namespace payments {

// static
bool PaymentCredential::IsFrameAllowedToUseSecurePaymentConfirmation(
    content::RenderFrameHost* rfh) {
  return rfh && rfh->IsCurrent() &&
         rfh->IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kPayment) &&
         base::FeatureList::IsEnabled(features::kSecurePaymentConfirmation);
}

PaymentCredential::PaymentCredential(
    content::WebContents* web_contents,
    content::GlobalFrameRoutingId initiator_frame_routing_id,
    scoped_refptr<PaymentManifestWebDataService> web_data_service,
    mojo::PendingReceiver<mojom::PaymentCredential> receiver)
    : WebContentsObserver(web_contents),
      initiator_frame_routing_id_(initiator_frame_routing_id),
      web_data_service_(web_data_service) {
  DCHECK(web_contents);
  receiver_.Bind(std::move(receiver));
}

PaymentCredential::~PaymentCredential() {
  AbortAndCleanup();
}

void PaymentCredential::DownloadIconAndShowUserPrompt(
    payments::mojom::PaymentCredentialInstrumentPtr instrument,
    DownloadIconAndShowUserPromptCallback callback) {
  if (state_ != State::kIdle || !IsCurrentStateValid() || !instrument ||
      instrument->display_name.empty() ||
      !UrlUtil::IsOriginAllowedToUseWebPaymentApis(instrument->icon)) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  // Only one enrollment UI per WebContents at a time.
  ui_controller_ =
      PaymentCredentialEnrollmentController::GetOrCreateForWebContents(
          web_contents())
          ->GetWeakPtr();
  ui_controller_token_ = ui_controller_->GetTokenIfAvailable();
  if (!ui_controller_token_) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  state_ = State::kDownloadingIcon;
  pending_icon_download_request_id_ = web_contents()->DownloadImageInFrame(
      initiator_frame_routing_id_,
      instrument->icon,  // source URL
      true,              // is_favicon
      0,                 // no preferred size
      0,                 // no max size
      false,             // normal cache policy (a.k.a. do not bypass cache)
      base::BindOnce(&PaymentCredential::DidDownloadIcon,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentCredential::StorePaymentCredentialAndHideUserPrompt(
    payments::mojom::PaymentCredentialInstrumentPtr instrument,
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    StorePaymentCredentialAndHideUserPromptCallback callback) {
  if (state_ != State::kMakingCredential || !IsCurrentStateValid()) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_INSTRUMENT);
    return;
  }

  state_ = State::kStoringCredential;
  WebDataServiceBase::Handle handle =
      web_data_service_->AddSecurePaymentConfirmationInstrument(
          std::make_unique<SecurePaymentConfirmationInstrument>(
              credential_id, rp_id, base::UTF8ToUTF16(instrument->display_name),
              std::move(encoded_icon_)),
          /*consumer=*/this);
  storage_callbacks_[handle] = std::move(callback);
}

void PaymentCredential::HideUserPrompt(HideUserPromptCallback callback) {
  DCHECK_EQ(State::kMakingCredential, state_);
  DCHECK(IsCurrentStateValid());

  AbortAndCleanup();
  std::move(callback).Run();
}

void PaymentCredential::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  auto iterator = storage_callbacks_.find(h);
  if (iterator == storage_callbacks_.end()) {
    AbortAndCleanup();
    return;
  }

  auto callback = std::move(iterator->second);
  DCHECK(callback);
  storage_callbacks_.erase(iterator);

  if (state_ != State::kStoringCredential || !IsCurrentStateValid()) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_INSTRUMENT);
    return;
  }

  ui_controller_->CloseDialog();
  ui_controller_.reset();

  state_ = State::kIdle;
  std::move(callback).Run(
      static_cast<WDResult<bool>*>(result.get())->GetValue()
          ? mojom::PaymentCredentialStorageStatus::SUCCESS
          : mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_INSTRUMENT);
}

bool PaymentCredential::IsCurrentStateValid() const {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(initiator_frame_routing_id_);

  if (!IsFrameAllowedToUseSecurePaymentConfirmation(render_frame_host) ||
      !web_contents() ||
      web_contents() !=
          content::WebContents::FromRenderFrameHost(render_frame_host) ||
      !web_data_service_ || !storage_callbacks_.empty() ||
      !receiver_.is_bound()) {
    return false;
  }

  switch (state_) {
    case State::kIdle:
      return !ui_controller_ && !ui_controller_token_ &&
             !pending_icon_download_request_id_ && encoded_icon_.empty();

    case State::kDownloadingIcon:
      return ui_controller_ && ui_controller_token_ &&
             pending_icon_download_request_id_ && encoded_icon_.empty();

    case State::kShowingUserPrompt:
      FALLTHROUGH;
    case State::kMakingCredential:
      return ui_controller_ && ui_controller_token_ &&
             !pending_icon_download_request_id_ && !encoded_icon_.empty();

    case State::kStoringCredential:
      return ui_controller_ && ui_controller_token_ &&
             !pending_icon_download_request_id_ && encoded_icon_.empty();
  }
}

void PaymentCredential::DidDownloadIcon(
    DownloadIconAndShowUserPromptCallback callback,
    int request_id,
    int unused_http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& unused_sizes) {
  if (state_ != State::kDownloadingIcon || !IsCurrentStateValid() ||
      bitmaps.empty()) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  DCHECK_EQ(pending_icon_download_request_id_.value(), request_id);
  pending_icon_download_request_id_.reset();

  // TODO(https://crbug.com/1110320): Get the best icon using |preferred size|
  // rather than the first one if multiple downloaded.
  gfx::Image downloaded_image = gfx::Image::CreateFrom1xBitmap(bitmaps.front());
  scoped_refptr<base::RefCountedMemory> raw_data =
      downloaded_image.As1xPNGBytes();
  encoded_icon_ =
      std::vector<uint8_t>(raw_data->front_as<uint8_t>(),
                           raw_data->front_as<uint8_t>() + raw_data->size());

  state_ = State::kShowingUserPrompt;
  ui_controller_->ShowDialog(
      initiator_frame_routing_id_,
      std::make_unique<SkBitmap>(std::move(bitmaps.front())),
      base::BindOnce(&PaymentCredential::OnUserResponseFromUI,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentCredential::OnUserResponseFromUI(
    DownloadIconAndShowUserPromptCallback callback,
    bool user_confirm_from_ui) {
  if (state_ != State::kShowingUserPrompt || !IsCurrentStateValid() ||
      !user_confirm_from_ui) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::USER_CANCEL_FROM_UI);
    return;
  }

  state_ = State::kMakingCredential;
  std::move(callback).Run(
      mojom::PaymentCredentialUserPromptStatus::USER_CONFIRM_FROM_UI);
}

void PaymentCredential::AbortAndCleanup() {
  if (web_data_service_) {
    std::for_each(storage_callbacks_.begin(), storage_callbacks_.end(),
                  [&](const auto& pair) {
                    web_data_service_->CancelRequest(pair.first);
                  });
  }
  storage_callbacks_.clear();
  encoded_icon_.clear();
  pending_icon_download_request_id_.reset();
  ui_controller_token_.reset();
  if (ui_controller_)
    ui_controller_->CloseDialog();
  ui_controller_.reset();
  state_ = State::kIdle;
}

}  // namespace payments
