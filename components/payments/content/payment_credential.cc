// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

#include <algorithm>
#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/content/payment_credential_enrollment_controller.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/payments/core/url_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"

namespace payments {

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
  if (!web_contents() || !instrument || instrument->display_name.empty() ||
      controller_ || IsDialogShowing() ||
      !UrlUtil::IsOriginAllowedToUseWebPaymentApis(instrument->icon)) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  // If the initiator frame doesn't exist any more, e.g. the frame has
  // navigated away, don't download the icon.
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(initiator_frame_routing_id_);
  if (!render_frame_host || !render_frame_host->IsCurrent()) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  // Only one PaymentCredential enrollment at a time.
  if (pending_icon_download_request_id_) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

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
  if (!web_data_service_ || !controller_) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_INSTRUMENT);
    return;
  }

  WebDataServiceBase::Handle handle =
      web_data_service_->AddSecurePaymentConfirmationInstrument(
          std::make_unique<SecurePaymentConfirmationInstrument>(
              credential_id, rp_id, base::UTF8ToUTF16(instrument->display_name),
              std::move(encoded_icon_)),
          /*consumer=*/this);
  callbacks_[handle] = std::move(callback);
}

void PaymentCredential::HideUserPrompt(HideUserPromptCallback callback) {
  AbortAndCleanup();
  std::move(callback).Run();
}

void PaymentCredential::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  auto iterator = callbacks_.find(h);
  if (iterator == callbacks_.end()) {
    AbortAndCleanup();
    return;
  }

  auto callback = std::move(iterator->second);
  DCHECK(callback);
  callbacks_.erase(iterator);

  if (!controller_) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_INSTRUMENT);
    return;
  }

  controller_->CloseDialog();
  controller_.reset();

  std::move(callback).Run(
      static_cast<WDResult<bool>*>(result.get())->GetValue()
          ? mojom::PaymentCredentialStorageStatus::SUCCESS
          : mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_INSTRUMENT);
}

bool PaymentCredential::IsDialogShowing() const {
  auto* controller =
      payments::PaymentCredentialEnrollmentController::FromWebContents(
          web_contents());
  return controller && controller->IsShowing();
}

void PaymentCredential::DidDownloadIcon(
    DownloadIconAndShowUserPromptCallback callback,
    int request_id,
    int unused_http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& unused_sizes) {
  DCHECK(pending_icon_download_request_id_.has_value());
  DCHECK_EQ(pending_icon_download_request_id_.value(), request_id);
  pending_icon_download_request_id_.reset();

  if (bitmaps.empty() || !web_contents() || controller_ || IsDialogShowing()) {
    AbortAndCleanup();
    std::move(callback).Run(
        mojom::PaymentCredentialUserPromptStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  // TODO(https://crbug.com/1110320): Get the best icon using |preferred size|
  // rather than the first one if multiple downloaded.
  gfx::Image downloaded_image = gfx::Image::CreateFrom1xBitmap(bitmaps[0]);
  scoped_refptr<base::RefCountedMemory> raw_data =
      downloaded_image.As1xPNGBytes();
  encoded_icon_ =
      std::vector<uint8_t>(raw_data->front_as<uint8_t>(),
                           raw_data->front_as<uint8_t>() + raw_data->size());

  PaymentCredentialEnrollmentController::CreateForWebContents(web_contents());
  controller_ =
      PaymentCredentialEnrollmentController::FromWebContents(web_contents())
          ->GetWeakPtr();
  controller_->ShowDialog(
      base::BindOnce(&PaymentCredential::OnUserResponseFromUI,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentCredential::OnUserResponseFromUI(
    DownloadIconAndShowUserPromptCallback callback,
    bool user_confirm_from_ui) {
  if (!user_confirm_from_ui)
    AbortAndCleanup();

  std::move(callback).Run(
      user_confirm_from_ui
          ? mojom::PaymentCredentialUserPromptStatus::USER_CONFIRM_FROM_UI
          : mojom::PaymentCredentialUserPromptStatus::USER_CANCEL_FROM_UI);
}

void PaymentCredential::AbortAndCleanup() {
  if (web_data_service_) {
    std::for_each(callbacks_.begin(), callbacks_.end(), [&](const auto& pair) {
      web_data_service_->CancelRequest(pair.first);
    });
  }
  callbacks_.clear();
  encoded_icon_.clear();
  pending_icon_download_request_id_.reset();
  if (controller_)
    controller_->CloseDialog();
  controller_.reset();
}

}  // namespace payments
