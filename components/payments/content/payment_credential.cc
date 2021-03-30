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
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "ui/gfx/image/image.h"

namespace payments {

// static
bool PaymentCredential::IsFrameAllowedToUseSecurePaymentConfirmation(
    content::RenderFrameHost* rfh) {
  return rfh && rfh->IsCurrent() &&
         rfh->IsFeatureEnabled(
             blink::mojom::PermissionsPolicyFeature::kPayment) &&
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
  Reset();
}

void PaymentCredential::DownloadIconAndShowUserPrompt(
    payments::mojom::PaymentCredentialInstrumentPtr instrument,
    DownloadIconAndShowUserPromptCallback callback) {
  if (state_ != State::kIdle || !IsCurrentStateValid() || !instrument ||
      instrument->display_name.empty() ||
      !UrlUtil::IsOriginAllowedToUseWebPaymentApis(instrument->icon)) {
    RecordFirstDialogShown(
        SecurePaymentConfirmationEnrollDialogShown::kCouldNotShow);
    Reset();
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
    RecordFirstDialogShown(
        SecurePaymentConfirmationEnrollDialogShown::kCouldNotShow);
    Reset();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  prompt_callback_ = std::move(callback);
  state_ = State::kDownloadingIcon;
  pending_icon_download_request_id_ = web_contents()->DownloadImageInFrame(
      initiator_frame_routing_id_,
      instrument->icon,  // source URL
      true,              // is_favicon
      0,                 // no preferred size
      0,                 // no max size
      false,             // normal cache policy (a.k.a. do not bypass cache)
      base::BindOnce(&PaymentCredential::DidDownloadIcon,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::UTF8ToUTF16(instrument->display_name)));
}

void PaymentCredential::StorePaymentCredentialAndHideUserPrompt(
    payments::mojom::PaymentCredentialInstrumentPtr instrument,
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    StorePaymentCredentialAndHideUserPromptCallback callback) {
  if (state_ != State::kMakingCredential || !IsCurrentStateValid() ||
      !instrument || instrument->display_name.empty() ||
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
          std::make_unique<SecurePaymentConfirmationInstrument>(
              credential_id, rp_id, base::UTF8ToUTF16(instrument->display_name),
              std::move(encoded_icon_)),
          /*consumer=*/this);
}

void PaymentCredential::HideUserPrompt(HideUserPromptCallback callback) {
  if (state_ == State::kMakingCredential) {
    RecordFirstSystemPromptResult(
        SecurePaymentConfirmationEnrollSystemPromptResult::kCanceled);
  } else {
    NOTREACHED();
  }
  DCHECK(IsCurrentStateValid());

  Reset();
  std::move(callback).Run();
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
  if (!navigation_handle->IsSameDocument() &&
      (navigation_handle->IsInMainFrame() ||
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
      return !prompt_callback_ && !storage_callback_ &&
             !data_service_request_handle_ && !ui_controller_ &&
             !ui_controller_token_ && !pending_icon_download_request_id_ &&
             encoded_icon_.empty();

    case State::kDownloadingIcon:
      return prompt_callback_ && !storage_callback_ &&
             !data_service_request_handle_ && ui_controller_ &&
             ui_controller_token_ && pending_icon_download_request_id_ &&
             encoded_icon_.empty();

    case State::kShowingUserPrompt:
      return prompt_callback_ && !storage_callback_ &&
             !data_service_request_handle_ && ui_controller_ &&
             ui_controller_token_ && !pending_icon_download_request_id_ &&
             !encoded_icon_.empty();

    case State::kMakingCredential:
      return !prompt_callback_ && !storage_callback_ &&
             !data_service_request_handle_ && ui_controller_ &&
             ui_controller_token_ && !pending_icon_download_request_id_ &&
             !encoded_icon_.empty();

    case State::kStoringCredential:
      return !prompt_callback_ && storage_callback_ &&
             data_service_request_handle_ && ui_controller_ &&
             ui_controller_token_ && !pending_icon_download_request_id_ &&
             encoded_icon_.empty();
  }
}

void PaymentCredential::DidDownloadIcon(
    const std::u16string instrument_name,
    int request_id,
    int unused_http_status_code,
    const GURL& unused_image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& unused_sizes) {
  if (state_ != State::kDownloadingIcon || !IsCurrentStateValid() ||
      instrument_name.empty() ||
      request_id != pending_icon_download_request_id_.value() ||
      bitmaps.empty()) {
    RecordFirstDialogShown(
        SecurePaymentConfirmationEnrollDialogShown::kCouldNotShow);
    Reset();
    return;
  }

  pending_icon_download_request_id_.reset();

  // TODO(https://crbug.com/1110320): Get the best icon using |preferred size|
  // rather than the first one if multiple downloaded.
  gfx::Image downloaded_image = gfx::Image::CreateFrom1xBitmap(bitmaps.front());
  scoped_refptr<base::RefCountedMemory> raw_data =
      downloaded_image.As1xPNGBytes();
  encoded_icon_ =
      std::vector<uint8_t>(raw_data->front_as<uint8_t>(),
                           raw_data->front_as<uint8_t>() + raw_data->size());
  RecordFirstDialogShown(SecurePaymentConfirmationEnrollDialogShown::kShown);

  state_ = State::kShowingUserPrompt;
  ui_controller_->ShowDialog(
      initiator_frame_routing_id_,
      std::make_unique<SkBitmap>(std::move(bitmaps.front())), instrument_name,
      base::BindOnce(&PaymentCredential::OnUserResponseFromUI,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PaymentCredential::OnUserResponseFromUI(bool user_confirm_from_ui) {
  if (state_ != State::kShowingUserPrompt || !IsCurrentStateValid() ||
      !user_confirm_from_ui) {
    Reset();
    return;
  }

  state_ = State::kMakingCredential;
  std::move(prompt_callback_)
      .Run(mojom::PaymentCredentialUserPromptStatus::USER_CONFIRM_FROM_UI);
}

void PaymentCredential::RecordFirstDialogShown(
    SecurePaymentConfirmationEnrollDialogShown shown) {
  if (!is_dialog_shown_recorded_) {
    is_dialog_shown_recorded_ = true;
    RecordEnrollDialogShown(shown);
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
    if (prompt_callback_) {
      std::move(prompt_callback_)
          .Run(state_ == State::kShowingUserPrompt
                   ? mojom::PaymentCredentialUserPromptStatus::
                         USER_CANCEL_FROM_UI
                   : mojom::PaymentCredentialUserPromptStatus::
                         FAILED_TO_DOWNLOAD_ICON);
    }
  }

  if (web_data_service_ && data_service_request_handle_) {
    web_data_service_->CancelRequest(data_service_request_handle_.value());
  }

  if (state_ == State::kDownloadingIcon) {
    RecordFirstDialogShown(
        SecurePaymentConfirmationEnrollDialogShown::kCouldNotShow);
  }

  if (state_ == State::kMakingCredential) {
    RecordFirstSystemPromptResult(
        SecurePaymentConfirmationEnrollSystemPromptResult::kCanceled);
  }

  data_service_request_handle_.reset();
  encoded_icon_.clear();
  pending_icon_download_request_id_.reset();
  ui_controller_token_.reset();
  if (ui_controller_)
    ui_controller_->CloseDialog();
  ui_controller_.reset();
  is_dialog_shown_recorded_ = false;
  is_system_prompt_result_recorded_ = false;
  state_ = State::kIdle;
}

}  // namespace payments
