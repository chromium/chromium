// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

#include <algorithm>
#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
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
  if (web_data_service_) {
    std::for_each(callbacks_.begin(), callbacks_.end(), [&](const auto& pair) {
      web_data_service_->CancelRequest(pair.first);
    });
  }
}

void PaymentCredential::StorePaymentCredential(
    payments::mojom::PaymentCredentialInstrumentPtr instrument,
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    StorePaymentCredentialCallback callback) {
  if (!web_data_service_) {
    std::move(callback).Run(
        mojom::PaymentCredentialCreationStatus::FAILED_TO_STORE_INSTRUMENT);
    return;
  }

  if (!web_contents() ||
      !UrlUtil::IsOriginAllowedToUseWebPaymentApis(instrument->icon)) {
    std::move(callback).Run(
        mojom::PaymentCredentialCreationStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  // If the initiator frame doesn't exist any more, e.g. the frame has
  // navigated away, don't download the icon.
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(initiator_frame_routing_id_);
  if (!render_frame_host || !render_frame_host->IsCurrent()) {
    std::move(callback).Run(
        mojom::PaymentCredentialCreationStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  const GURL icon_url = instrument->icon;
  int request_id = web_contents()->DownloadImageInFrame(
      initiator_frame_routing_id_,
      icon_url,  // source URL
      true,      // is_favicon
      0,         // no preferred size
      0,         // no max size
      false,     // normal cache policy (a.k.a. do not bypass cache)
      base::BindOnce(&PaymentCredential::DidDownloadFavicon,
                     weak_ptr_factory_.GetWeakPtr(), std::move(instrument),
                     credential_id, rp_id, std::move(callback)));
  pending_icon_download_request_ids_.insert(request_id);
}

void PaymentCredential::DidDownloadFavicon(
    payments::mojom::PaymentCredentialInstrumentPtr instrument,
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    StorePaymentCredentialCallback callback,
    int request_id,
    int unused_http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& unused_sizes) {
  auto iterator = pending_icon_download_request_ids_.find(request_id);
  DCHECK(iterator != pending_icon_download_request_ids_.end());
  pending_icon_download_request_ids_.erase(iterator);

  if (bitmaps.empty()) {
    std::move(callback).Run(
        mojom::PaymentCredentialCreationStatus::FAILED_TO_DOWNLOAD_ICON);
    return;
  }

  // TODO(https://crbug.com/1110320): Get the best icon using |preferred size|
  // rather than the first one if multiple downloaded.
  gfx::Image downloaded_image = gfx::Image::CreateFrom1xBitmap(bitmaps[0]);
  scoped_refptr<base::RefCountedMemory> raw_data =
      downloaded_image.As1xPNGBytes();
  auto encoded_icon =
      std::vector<uint8_t>(raw_data->front_as<uint8_t>(),
                           raw_data->front_as<uint8_t>() + raw_data->size());

  WebDataServiceBase::Handle handle =
      web_data_service_->AddSecurePaymentConfirmationInstrument(
          std::make_unique<SecurePaymentConfirmationInstrument>(
              credential_id, rp_id, base::UTF8ToUTF16(instrument->display_name),
              std::move(encoded_icon)),
          /*consumer=*/this);
  callbacks_[handle] = std::move(callback);
}

void PaymentCredential::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  auto iterator = callbacks_.find(h);
  if (iterator == callbacks_.end())
    return;

  auto callback = std::move(iterator->second);
  DCHECK(callback);
  callbacks_.erase(iterator);

  std::move(callback).Run(
      static_cast<WDResult<bool>*>(result.get())->GetValue()
          ? mojom::PaymentCredentialCreationStatus::SUCCESS
          : mojom::PaymentCredentialCreationStatus::FAILED_TO_STORE_INSTRUMENT);
}

}  // namespace payments
