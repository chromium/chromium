// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app.h"

#include <utility>

#include "base/check.h"
#include "base/containers/flat_tree.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payer_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "url/url_constants.h"

namespace payments {
namespace {

static constexpr int kDefaultTimeoutMinutes = 3;

}  // namespace

SecurePaymentConfirmationApp::SecurePaymentConfirmationApp(
    const std::string& effective_relying_party_identity,
    std::unique_ptr<SkBitmap> icon,
    const base::string16& label,
    std::vector<std::unique_ptr<std::vector<uint8_t>>> credential_ids,
    const url::Origin& merchant_origin,
    const mojom::PaymentCurrencyAmountPtr& total,
    mojom::SecurePaymentConfirmationRequestPtr request,
    std::unique_ptr<autofill::InternalAuthenticator> authenticator)
    : PaymentApp(/*icon_resource_id=*/0, PaymentApp::Type::INTERNAL),
      effective_relying_party_identity_(effective_relying_party_identity),
      icon_(std::move(icon)),
      label_(label),
      credential_ids_(std::move(credential_ids)),
      merchant_origin_(merchant_origin),
      total_(total.Clone()),
      request_(std::move(request)),
      authenticator_(std::move(authenticator)) {
  DCHECK(!credential_ids_.empty());
  DCHECK(credential_ids_.front());
  DCHECK(!credential_ids_.front()->empty());

  app_method_names_.insert(methods::kSecurePaymentConfirmation);
}

SecurePaymentConfirmationApp::~SecurePaymentConfirmationApp() = default;

void SecurePaymentConfirmationApp::InvokePaymentApp(Delegate* delegate) {
  std::vector<device::PublicKeyCredentialDescriptor> credentials;
  for (const auto& credential_id : credential_ids_) {
    credentials.emplace_back(device::CredentialType::kPublicKey, *credential_id,
                             base::flat_set<device::FidoTransportProtocol>{
                                 device::FidoTransportProtocol::kInternal});
  }

  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->relying_party_id = effective_relying_party_identity_;
  options->timeout = request_->timeout.has_value()
                         ? request_->timeout.value()
                         : base::TimeDelta::FromMinutes(kDefaultTimeoutMinutes);
  options->user_verification = device::UserVerificationRequirement::kRequired;
  options->allow_credentials = std::move(credentials);

  // TODO(https://crbug.com/1110324): Combine |merchant_origin_|, |total_|, and
  // |request_->network_data| into a challenge to invoke the authenticator.
  options->challenge = request_->network_data;

  // We are nullifying the security check by design, and the origin that created
  // the credential isn't saved anywhere.
  authenticator_->SetEffectiveOrigin(url::Origin::Create(
      GURL(base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                         effective_relying_party_identity_}))));

  authenticator_->GetAssertion(
      std::move(options),
      base::BindOnce(&SecurePaymentConfirmationApp::OnGetAssertion,
                     weak_ptr_factory_.GetWeakPtr(), delegate));
}

bool SecurePaymentConfirmationApp::IsCompleteForPayment() const {
  return true;
}

uint32_t SecurePaymentConfirmationApp::GetCompletenessScore() const {
  // This value is used for sorting multiple apps, but this app always appears
  // on its own.
  return 0;
}

bool SecurePaymentConfirmationApp::CanPreselect() const {
  return true;
}

base::string16 SecurePaymentConfirmationApp::GetMissingInfoLabel() const {
  NOTREACHED();
  return base::string16();
}

bool SecurePaymentConfirmationApp::HasEnrolledInstrument() const {
  // If there's no platform authenticator, then the factory should not create
  // this app. Therefore, this function can always return true.
  return true;
}

void SecurePaymentConfirmationApp::RecordUse() {
  NOTIMPLEMENTED();
}

bool SecurePaymentConfirmationApp::NeedsInstallation() const {
  return false;
}

std::string SecurePaymentConfirmationApp::GetId() const {
  return request_->instrument_id;
}

base::string16 SecurePaymentConfirmationApp::GetLabel() const {
  return label_;
}

base::string16 SecurePaymentConfirmationApp::GetSublabel() const {
  return base::string16();
}

const SkBitmap* SecurePaymentConfirmationApp::icon_bitmap() const {
  return icon_.get();
}

bool SecurePaymentConfirmationApp::IsValidForModifier(
    const std::string& method,
    bool supported_networks_specified,
    const std::set<std::string>& supported_networks) const {
  bool is_valid = false;
  IsValidForPaymentMethodIdentifier(method, &is_valid);
  return is_valid;
}

base::WeakPtr<PaymentApp> SecurePaymentConfirmationApp::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool SecurePaymentConfirmationApp::HandlesShippingAddress() const {
  return false;
}

bool SecurePaymentConfirmationApp::HandlesPayerName() const {
  return false;
}

bool SecurePaymentConfirmationApp::HandlesPayerEmail() const {
  return false;
}

bool SecurePaymentConfirmationApp::HandlesPayerPhone() const {
  return false;
}

bool SecurePaymentConfirmationApp::IsWaitingForPaymentDetailsUpdate() const {
  return false;
}

void SecurePaymentConfirmationApp::UpdateWith(
    mojom::PaymentRequestDetailsUpdatePtr details_update) {
  NOTREACHED();
}

void SecurePaymentConfirmationApp::OnPaymentDetailsNotUpdated() {
  NOTREACHED();
}

void SecurePaymentConfirmationApp::AbortPaymentApp(
    base::OnceCallback<void(bool)> abort_callback) {
  authenticator_->Cancel();
  std::move(abort_callback).Run(/*abort_success=*/true);
}

void SecurePaymentConfirmationApp::OnGetAssertion(
    Delegate* delegate,
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response) {
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    delegate->OnInstrumentDetailsError("Authentication failure.");
    return;
  }

  // TODO(https://crbug.com/1110324): Serialize response into a JSON string.
  // Browser will pass this string over Mojo IPC into Blink, which will parse it
  // into a JavaScript object for the merchant.
  std::string json_serialized_response = "{\"status\": \"success\"}";

  delegate->OnInstrumentDetailsReady(methods::kSecurePaymentConfirmation,
                                     json_serialized_response, PayerData());
}

}  // namespace payments
