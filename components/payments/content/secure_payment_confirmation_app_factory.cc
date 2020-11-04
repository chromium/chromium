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
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/decode_image.h"
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

  return true;
}

}  // namespace

void SecurePaymentConfirmationAppFactory::
    OnIsUserVerifyingPlatformAuthenticatorAvailable(
        base::WeakPtr<PaymentAppFactory::Delegate> delegate,
        mojom::SecurePaymentConfirmationRequestPtr request,
        std::unique_ptr<autofill::InternalAuthenticator> authenticator,
        bool is_available) {
  if (!delegate || !delegate->GetWebContents())
    return;

  if (!is_available && !base::FeatureList::IsEnabled(
                           features::kSecurePaymentConfirmationDebug)) {
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
  requests_[handle] = std::make_unique<Request>(
      delegate, web_data_service, std::move(request), std::move(authenticator));
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

      std::unique_ptr<autofill::InternalAuthenticator> authenticator =
          delegate->CreateInternalAuthenticator();
      auto* authenticator_ptr = authenticator.get();

      authenticator_ptr->IsUserVerifyingPlatformAuthenticatorAvailable(
          base::BindOnce(&SecurePaymentConfirmationAppFactory::
                             OnIsUserVerifyingPlatformAuthenticatorAvailable,
                         weak_ptr_factory_.GetWeakPtr(), delegate,
                         method_data->secure_payment_confirmation.Clone(),
                         std::move(authenticator)));
      return;
    }
  }

  delegate->OnDoneCreatingPaymentApps();
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

  base::WeakPtr<PaymentAppFactory::Delegate> delegate;
  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service;
  mojom::SecurePaymentConfirmationRequestPtr mojo_request;
  std::unique_ptr<autofill::InternalAuthenticator> authenticator;
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
  if (instruments.empty()) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  // For the pilot phase, arbitrarily use the first matching instrument.
  // TODO(https://crbug.com/1110320): Handle multiple instruments.
  std::unique_ptr<SecurePaymentConfirmationInstrument> instrument =
      std::move(instruments.front());

  auto* instrument_ptr = instrument.get();
  // Decode the icon in a sandboxed process off the main thread.
  data_decoder::DecodeImageIsolated(
      instrument_ptr->icon, data_decoder::mojom::ImageCodec::DEFAULT,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&SecurePaymentConfirmationAppFactory::OnAppIconDecoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(instrument),
                     std::move(request)));
}

void SecurePaymentConfirmationAppFactory::OnAppIconDecoded(
    std::unique_ptr<SecurePaymentConfirmationInstrument> instrument,
    std::unique_ptr<Request> request,
    const SkBitmap& decoded_icon) {
  DCHECK(request);
  if (!request->delegate || !request->web_contents() ||
      !request->delegate->GetSpec() ||
      request->authenticator->GetRenderFrameHost() !=
          request->web_contents()->GetMainFrame()) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  DCHECK(!decoded_icon.drawsNothing());
  auto icon = std::make_unique<SkBitmap>(decoded_icon);

  request->delegate->OnPaymentAppCreated(
      std::make_unique<SecurePaymentConfirmationApp>(
          request->web_contents(), instrument->relying_party_id,
          std::move(icon), instrument->label,
          std::move(instrument->credential_id),
          url::Origin::Create(request->delegate->GetTopOrigin()),
          request->delegate->GetSpec()->AsWeakPtr(),
          std::move(request->mojo_request), std::move(request->authenticator)));

  request->delegate->OnDoneCreatingPaymentApps();
}

}  // namespace payments
