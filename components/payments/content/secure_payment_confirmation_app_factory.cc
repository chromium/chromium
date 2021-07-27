// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app_factory.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/origin.h"

namespace payments {
namespace {

// Arbitrarily chosen limit of 1 hour. Keep in sync with
// secure_payment_confirmation_helper.cc.
constexpr int64_t kMaxTimeoutInMilliseconds = 1000 * 60 * 60;

bool IsValid(const mojom::SecurePaymentConfirmationRequestPtr& request,
             std::string* error_message) {
  // `request` can be null when the feature is disabled in Blink.
  if (!request)
    return false;

  if (request->credential_ids.empty()) {
    *error_message = errors::kCredentialIdsRequired;
    return false;
  }

  for (const auto& credential_id : request->credential_ids) {
    if (credential_id.empty()) {
      *error_message = errors::kCredentialIdsRequired;
      return false;
    }
  }

  if (request->timeout.has_value() &&
      request->timeout.value().InMilliseconds() > kMaxTimeoutInMilliseconds) {
    *error_message = errors::kTimeoutTooLong;
    return false;
  }

  if (request->challenge.empty()) {
    *error_message = errors::kChallengeRequired;
    return false;
  }

  if (!request->instrument) {
    *error_message = errors::kInstrumentRequired;
    return false;
  }

  if (request->instrument->display_name.empty()) {
    *error_message = errors::kInstrumentDisplayNameRequired;
    return false;
  }

  if (!request->instrument->icon.is_valid()) {
    *error_message = errors::kValidInstrumentIconRequired;
    return false;
  }

  return true;
}

}  // namespace

void SecurePaymentConfirmationAppFactory::
    OnIsUserVerifyingPlatformAuthenticatorAvailable(
        base::WeakPtr<PaymentAppFactory::Delegate> delegate,
        mojom::SecurePaymentConfirmationRequestPtr request,
        bool is_available) {
  if (!delegate || !delegate->GetWebContents())
    return;

  if (!authenticator_ ||
      (!is_available && !base::FeatureList::IsEnabled(
                            features::kSecurePaymentConfirmationDebug))) {
    delegate->OnDoneCreatingPaymentApps();
    return;
  }

  // Regardless of whether `web_data_service` has any apps, canMakePayment() and
  // hasEnrolledInstrument() should return true when a user-verifying platform
  // authenticator device is available.
  delegate->SetCanMakePaymentEvenWithoutApps();

  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      delegate->GetPaymentManifestWebDataService();
  if (!web_data_service) {
    delegate->OnDoneCreatingPaymentApps();
    return;
  }

  WebDataServiceBase::Handle handle =
      web_data_service->GetSecurePaymentConfirmationInstruments(
          std::move(request->credential_ids), this);
  requests_[handle] =
      std::make_unique<Request>(delegate, web_data_service, std::move(request),
                                std::move(authenticator_));
}

SecurePaymentConfirmationAppFactory::SecurePaymentConfirmationAppFactory()
    : PaymentAppFactory(PaymentApp::Type::INTERNAL) {}

SecurePaymentConfirmationAppFactory::~SecurePaymentConfirmationAppFactory() {
  std::for_each(requests_.begin(), requests_.end(), [&](const auto& pair) {
    if (pair.second->web_data_service)
      pair.second->web_data_service->CancelRequest(pair.first);
  });
}

void SecurePaymentConfirmationAppFactory::Create(
    base::WeakPtr<Delegate> delegate) {
  DCHECK(delegate);

  base::WeakPtr<PaymentRequestSpec> spec = delegate->GetSpec();
  if (!spec || !base::Contains(spec->payment_method_identifiers_set(),
                               methods::kSecurePaymentConfirmation)) {
    delegate->OnDoneCreatingPaymentApps();
    return;
  }

  for (const mojom::PaymentMethodDataPtr& method_data : spec->method_data()) {
    if (method_data->supported_method == methods::kSecurePaymentConfirmation) {
      std::string error_message;
      if (!IsValid(method_data->secure_payment_confirmation, &error_message)) {
        if (!error_message.empty())
          delegate->OnPaymentAppCreationError(error_message);
        delegate->OnDoneCreatingPaymentApps();
        return;
      }

      // Observe the web contents to ensure the authenticator outlives it.
      Observe(delegate->GetWebContents());

      authenticator_ = delegate->CreateInternalAuthenticator();

      authenticator_->IsUserVerifyingPlatformAuthenticatorAvailable(
          base::BindOnce(&SecurePaymentConfirmationAppFactory::
                             OnIsUserVerifyingPlatformAuthenticatorAvailable,
                         weak_ptr_factory_.GetWeakPtr(), delegate,
                         method_data->secure_payment_confirmation.Clone()));
      return;
    }
  }

  delegate->OnDoneCreatingPaymentApps();
}

void SecurePaymentConfirmationAppFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (authenticator_ &&
      authenticator_->GetRenderFrameHost() == render_frame_host) {
    authenticator_.reset();
  }
}

struct SecurePaymentConfirmationAppFactory::Request
    : public content::WebContentsObserver {
  Request(
      base::WeakPtr<PaymentAppFactory::Delegate> delegate,
      scoped_refptr<payments::PaymentManifestWebDataService> web_data_service,
      mojom::SecurePaymentConfirmationRequestPtr mojo_request,
      std::unique_ptr<autofill::InternalAuthenticator> authenticator)
      : content::WebContentsObserver(delegate->GetWebContents()),
        delegate(delegate),
        web_data_service(web_data_service),
        mojo_request(std::move(mojo_request)),
        authenticator(std::move(authenticator)) {}

  ~Request() override = default;

  Request(const Request& other) = delete;
  Request& operator=(const Request& other) = delete;

  // WebContentsObserver:
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    if (authenticator &&
        authenticator->GetRenderFrameHost() == render_frame_host) {
      authenticator.reset();
    }
  }

  base::WeakPtr<PaymentAppFactory::Delegate> delegate;
  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service;
  mojom::SecurePaymentConfirmationRequestPtr mojo_request;
  std::unique_ptr<autofill::InternalAuthenticator> authenticator;
  absl::optional<int> pending_icon_download_request_id;
};

void SecurePaymentConfirmationAppFactory::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  auto iterator = requests_.find(handle);
  if (iterator == requests_.end())
    return;

  std::unique_ptr<Request> request = std::move(iterator->second);
  requests_.erase(iterator);
  DCHECK(request.get());
  if (!request->delegate || !request->web_contents())
    return;

  if (!result || result->GetType() != SECURE_PAYMENT_CONFIRMATION) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  std::vector<std::unique_ptr<SecurePaymentConfirmationInstrument>>
      instruments = static_cast<WDResult<
          std::vector<std::unique_ptr<SecurePaymentConfirmationInstrument>>>*>(
                        result.get())
                        ->GetValue();
  std::unique_ptr<SecurePaymentConfirmationInstrument> instrument;

  // For the pilot phase, arbitrarily use the first matching instrument.
  // TODO(https://crbug.com/1110320): Handle multiple instruments.
  if (!instruments.empty())
    instrument = std::move(instruments.front());

  // Download the (possibly) updated icon for the payment instrument. The
  // download URL was passed into the PaymentRequest API.
  //
  // Perform this download regardless of whether there is an instrument on
  // file, so the server that hosts the image cannot detect presence of the
  // instrument on file.
  auto* request_ptr = request.get();
  request_ptr->pending_icon_download_request_id =
      request_ptr->web_contents()->DownloadImageInFrame(
          request_ptr->delegate->GetInitiatorRenderFrameHostId(),
          request_ptr->mojo_request->instrument->icon,  // source URL
          false,                                        // is_favicon
          0,                                            // no preferred size
          0,                                            // no max size
          false,  // normal cache policy (a.k.a. do not bypass cache)
          base::BindOnce(&SecurePaymentConfirmationAppFactory::DidDownloadIcon,
                         weak_ptr_factory_.GetWeakPtr(), std::move(instrument),
                         std::move(request)));
}

void SecurePaymentConfirmationAppFactory::OnAppIcon(
    std::unique_ptr<SecurePaymentConfirmationInstrument> instrument,
    std::unique_ptr<Request> request,
    const SkBitmap& icon) {
  DCHECK(request);
  if (!request->delegate || !request->web_contents())
    return;

  if (!request->delegate->GetSpec() || !request->authenticator ||
      request->authenticator->GetRenderFrameHost() !=
          request->web_contents()->GetMainFrame() ||
      !instrument) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  if (icon.drawsNothing()) {
    request->delegate->OnPaymentAppCreationError(errors::kInvalidIcon);
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  std::u16string label =
      base::UTF8ToUTF16(request->mojo_request->instrument->display_name);

  request->delegate->OnPaymentAppCreated(
      std::make_unique<SecurePaymentConfirmationApp>(
          request->web_contents(), instrument->relying_party_id,
          std::make_unique<SkBitmap>(icon), label,
          std::move(instrument->credential_id),
          url::Origin::Create(request->delegate->GetTopOrigin()),
          request->delegate->GetSpec()->AsWeakPtr(),
          std::move(request->mojo_request), std::move(request->authenticator)));

  request->delegate->OnDoneCreatingPaymentApps();
}

void SecurePaymentConfirmationAppFactory::DidDownloadIcon(
    std::unique_ptr<SecurePaymentConfirmationInstrument> instrument,
    std::unique_ptr<Request> request,
    int request_id,
    int unused_http_status_code,
    const GURL& unused_image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& unused_sizes) {
  DCHECK(request);
  bool has_icon =
      request->pending_icon_download_request_id.has_value() &&
      request->pending_icon_download_request_id.value() == request_id &&
      !bitmaps.empty();
  OnAppIcon(std::move(instrument), std::move(request),
            has_icon ? bitmaps.front() : SkBitmap());
}

}  // namespace payments
