// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app.h"

#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_tree.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payer_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "crypto/sha2.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "url/url_constants.h"

namespace payments {
namespace {

static constexpr int kDefaultTimeoutMinutes = 3;

// Creates a SHA-256 hash over the Secure Payment Confirmation bundle, which is
// a JSON string (without whitespace) with the following structure:
//   {
//     "merchantData" {
//       "merchantOrigin": "https://merchant.example",
//       "total": {
//         "currency": "CAD",
//         "value": "1.25",
//        },
//     },
//     "networkData": "YW=",
//   }
// where "networkData" is the base64 encoding of the `networkData` specified in
// the SecurePaymentConfirmationRequest. Sets the `challenge` out-param value to
// this JSON string.
std::vector<uint8_t> GetSecurePaymentConfirmationChallenge(
    const std::vector<uint8_t>& network_data,
    const url::Origin& merchant_origin,
    const mojom::PaymentCurrencyAmountPtr& amount,
    std::string* challenge) {
  base::Value total(base::Value::Type::DICTIONARY);
  total.SetKey("currency", base::Value(amount->currency));
  total.SetKey("value", base::Value(amount->value));

  base::Value merchant_data(base::Value::Type::DICTIONARY);
  merchant_data.SetKey("merchantOrigin",
                       base::Value(merchant_origin.Serialize()));
  merchant_data.SetKey("total", std::move(total));

  base::Value transaction_data(base::Value::Type::DICTIONARY);
  transaction_data.SetKey("networkData",
                          base::Value(base::Base64Encode(network_data)));
  transaction_data.SetKey("merchantData", std::move(merchant_data));

  bool success = base::JSONWriter::Write(transaction_data, challenge);
  DCHECK(success) << "Failed to write JSON for " << transaction_data;

  std::string sha256_hash = crypto::SHA256HashString(*challenge);
  std::vector<uint8_t> output_bytes(sha256_hash.begin(), sha256_hash.end());
  return output_bytes;
}

}  // namespace

SecurePaymentConfirmationApp::SecurePaymentConfirmationApp(
    content::WebContents* web_contents_to_observe,
    const std::string& effective_relying_party_identity,
    std::unique_ptr<SkBitmap> icon,
    const base::string16& label,
    std::vector<uint8_t> credential_id,
    const url::Origin& merchant_origin,
    base::WeakPtr<PaymentRequestSpec> spec,
    mojom::SecurePaymentConfirmationRequestPtr request,
    std::unique_ptr<autofill::InternalAuthenticator> authenticator)
    : PaymentApp(/*icon_resource_id=*/0, PaymentApp::Type::INTERNAL),
      content::WebContentsObserver(web_contents_to_observe),
      authenticator_render_frame_host_pointer_do_not_dereference_(
          authenticator->GetRenderFrameHost()),
      effective_relying_party_identity_(effective_relying_party_identity),
      icon_(std::move(icon)),
      label_(label),
      credential_id_(std::move(credential_id)),
      encoded_credential_id_(base::Base64Encode(credential_id_)),
      merchant_origin_(merchant_origin),
      spec_(spec),
      request_(std::move(request)),
      authenticator_(std::move(authenticator)) {
  DCHECK_EQ(web_contents_to_observe->GetMainFrame(),
            authenticator_render_frame_host_pointer_do_not_dereference_);
  DCHECK(!credential_id_.empty());

  app_method_names_.insert(methods::kSecurePaymentConfirmation);
}

SecurePaymentConfirmationApp::~SecurePaymentConfirmationApp() = default;

void SecurePaymentConfirmationApp::InvokePaymentApp(Delegate* delegate) {
  if (!authenticator_ || !spec_)
    return;

  DCHECK(spec_->IsInitialized());

  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->relying_party_id = effective_relying_party_identity_;
  options->timeout = request_->timeout.has_value()
                         ? request_->timeout.value()
                         : base::TimeDelta::FromMinutes(kDefaultTimeoutMinutes);
  options->user_verification = device::UserVerificationRequirement::kRequired;
  std::vector<device::PublicKeyCredentialDescriptor> credentials;

  if (base::FeatureList::IsEnabled(features::kSecurePaymentConfirmationDebug)) {
    options->user_verification =
        device::UserVerificationRequirement::kPreferred;
    // The `device::PublicKeyCredentialDescriptor` constructor with 2 parameters
    // enables authentication through all protocols.
    credentials.emplace_back(device::CredentialType::kPublicKey,
                             credential_id_);
  } else {
    // Enable authentication only through internal authenticators by default.
    credentials.emplace_back(device::CredentialType::kPublicKey, credential_id_,
                             base::flat_set<device::FidoTransportProtocol>{
                                 device::FidoTransportProtocol::kInternal});
  }

  options->allow_credentials = std::move(credentials);

  // Create a new challenge that is a hash of the transaction data.
  options->challenge = GetSecurePaymentConfirmationChallenge(
      request_->network_data, merchant_origin_,
      spec_->GetTotal(/*selected_app=*/this)->amount, &challenge_);

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
  return encoded_credential_id_;
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
  std::move(abort_callback).Run(/*abort_success=*/false);
}

void SecurePaymentConfirmationApp::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (authenticator_render_frame_host_pointer_do_not_dereference_ ==
      render_frame_host) {
    // The authenticator requires to be deleted before the render frame.
    authenticator_.reset();
  }
}

void SecurePaymentConfirmationApp::OnGetAssertion(
    Delegate* delegate,
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response) {
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS || !response) {
    std::stringstream status_string_stream;
    status_string_stream << status;
    delegate->OnInstrumentDetailsError(base::StringPrintf(
        "Authenticator returned %s.", status_string_stream.str().c_str()));
    return;
  }

  // Serialize response into a JSON string. Browser will pass this string over
  // Mojo IPC into Blink, which will parse it into a JavaScript object for the
  // merchant.
  auto info_json = std::make_unique<base::DictionaryValue>();
  if (response->info) {
    info_json->SetString("id", response->info->id);
    info_json->SetString("client_data_json",
                         base::Base64Encode(response->info->client_data_json));
    info_json->SetString(
        "authenticator_data",
        base::Base64Encode(response->info->authenticator_data));
  }

  auto prf_results_json = std::make_unique<base::DictionaryValue>();
  if (response->prf_results) {
    DCHECK(!response->prf_results->id.has_value());
    prf_results_json->SetString(
        "first", base::Base64Encode(response->prf_results->first));
    if (response->prf_results->second) {
      prf_results_json->SetString(
          "second", base::Base64Encode(*response->prf_results->second));
    }
  }

  base::DictionaryValue json;
  json.Set("info", std::move(info_json));
  json.SetString("challenge", challenge_);
  json.SetString("signature", base::Base64Encode(response->signature));
  if (response->user_handle.has_value()) {
    json.SetString("user_handle",
                   base::Base64Encode(response->user_handle.value()));
  }
  json.SetBoolean("echo_appid_extension", response->echo_appid_extension);
  json.SetBoolean("appid_extension", response->appid_extension);
  json.SetBoolean("echo_prf", response->echo_prf);
  json.Set("prf_results", std::move(prf_results_json));
  json.SetBoolean("prf_not_evaluated", response->echo_prf);

  std::string json_serialized_response;
  base::JSONWriter::Write(json, &json_serialized_response);
  delegate->OnInstrumentDetailsReady(methods::kSecurePaymentConfirmation,
                                     json_serialized_response, PayerData());
}

}  // namespace payments
