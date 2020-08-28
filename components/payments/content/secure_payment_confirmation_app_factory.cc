// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app_factory.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/origin.h"

namespace payments {
namespace {

// Arbitrarily chosen limit of 1 hour. Keep in sync with
// secure_payment_confirmation_helper.cc.
constexpr int64_t kMaxTimeoutInMilliseconds = 1000 * 60 * 60;

bool IsValid(const mojom::SecurePaymentConfirmationRequestPtr& request,
             std::string* error_message) {
  // |request| can be null when the feature is disabled in Blink.
  if (!request)
    return false;

  if (request->instrument_id.empty()) {
    *error_message = errors::kInstrumentIdRequired;
    return false;
  }

  if (request->timeout.has_value() &&
      request->timeout.value().InMilliseconds() > kMaxTimeoutInMilliseconds) {
    *error_message = errors::kTimeoutTooLong;
    return false;
  }

  return true;
}

void OnIsUserVerifyingPlatformAuthenticatorAvailable(
    base::WeakPtr<PaymentAppFactory::Delegate> delegate,
    mojom::SecurePaymentConfirmationRequestPtr request,
    std::unique_ptr<autofill::InternalAuthenticator> authenticator,
    bool is_available) {
  if (!delegate)
    return;

  if (!is_available) {
    delegate->OnDoneCreatingPaymentApps();
    return;
  }

  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      delegate->GetPaymentManifestWebDataService();
  if (!web_data_service) {
    delegate->OnDoneCreatingPaymentApps();
    return;
  }

  // TODO(https://crbug.com/1110324): Check |web_data_service| for whether
  // |request->instrument_id| has any credentials on this device. If so,
  // retrieve the instrument information from |web_data_service| and use these
  // values to create a SecurePaymentConfirmationApp. For now, use stubs.
  std::string effective_relying_party_identity = "rp.example";
  std::unique_ptr<SkBitmap> icon;
  base::string16 label = base::ASCIIToUTF16("Stub label");
  std::vector<std::unique_ptr<std::vector<uint8_t>>> credential_ids;
  credential_ids.emplace_back(std::make_unique<std::vector<uint8_t>>());
  credential_ids.back()->push_back(0);

  delegate->OnPaymentAppCreated(std::make_unique<SecurePaymentConfirmationApp>(
      effective_relying_party_identity, std::move(icon), label,
      std::move(credential_ids),
      /*merchant_origin=*/url::Origin::Create(delegate->GetTopOrigin()),
      /*total=*/delegate->GetSpec()->details().total->amount,
      std::move(request), std::move(authenticator)));
  delegate->OnDoneCreatingPaymentApps();
}

}  // namespace

SecurePaymentConfirmationAppFactory::SecurePaymentConfirmationAppFactory()
    : PaymentAppFactory(PaymentApp::Type::INTERNAL) {}

SecurePaymentConfirmationAppFactory::~SecurePaymentConfirmationAppFactory() =
    default;

void SecurePaymentConfirmationAppFactory::Create(
    base::WeakPtr<Delegate> delegate) {
  PaymentRequestSpec* spec = delegate->GetSpec();
  if (!base::Contains(spec->payment_method_identifiers_set(),
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

      authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
          base::BindOnce(&OnIsUserVerifyingPlatformAuthenticatorAvailable,
                         delegate,
                         method_data->secure_payment_confirmation.Clone(),
                         std::move(authenticator)));
      return;
    }
  }

  delegate->OnDoneCreatingPaymentApps();
}

}  // namespace payments
