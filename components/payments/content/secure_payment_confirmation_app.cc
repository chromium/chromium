// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/flat_tree.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payer_data.h"
#include "components/payments/core/payments_experimental_features.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "crypto/sha2.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/url_constants.h"

namespace payments {
namespace {

static constexpr int kDefaultTimeoutMinutes = 3;

// Records UMA metric for the system prompt result.
void RecordSystemPromptResult(
    const SecurePaymentConfirmationSystemPromptResult result) {
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.Funnel.SystemPromptResult",
      result);
}

#if BUILDFLAG(IS_ANDROID)
const device::PublicKeyCredentialParams::CredentialInfo
    kDefaultBrowserBoundKeyCredentialParameters[] = {
        {device::CredentialType::kPublicKey,
         base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256)},
        {device::CredentialType::kPublicKey,
         base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kRs256)}};
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

SecurePaymentConfirmationApp::SecurePaymentConfirmationApp(
    content::WebContents* web_contents_to_observe,
    const std::string& effective_relying_party_identity,
    const std::u16string& payment_instrument_label,
    std::unique_ptr<SkBitmap> payment_instrument_icon,
    std::vector<uint8_t> credential_id,
    std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder,
    const url::Origin& merchant_origin,
    base::WeakPtr<PaymentRequestSpec> spec,
    mojom::SecurePaymentConfirmationRequestPtr request,
    std::unique_ptr<webauthn::InternalAuthenticator> authenticator,
    const std::u16string& network_label,
    std::unique_ptr<SkBitmap> network_icon,
    const std::u16string& issuer_label,
    std::unique_ptr<SkBitmap> issuer_icon)
    : PaymentApp(/*icon_resource_id=*/0, PaymentApp::Type::INTERNAL),
      content::WebContentsObserver(web_contents_to_observe),
      authenticator_frame_routing_id_(
          authenticator ? authenticator->GetRenderFrameHost()->GetGlobalId()
                        : content::GlobalRenderFrameHostId()),
      effective_relying_party_identity_(effective_relying_party_identity),
      payment_instrument_label_(payment_instrument_label),
      payment_instrument_icon_(std::move(payment_instrument_icon)),
      credential_id_(std::move(credential_id)),
      merchant_origin_(merchant_origin),
      spec_(spec),
      request_(std::move(request)),
      authenticator_(std::move(authenticator)),
      passkey_browser_binder_(std::move(passkey_browser_binder)),
      network_label_(network_label),
      network_icon_(std::move(network_icon)),
      issuer_label_(issuer_label),
      issuer_icon_(std::move(issuer_icon)) {
  app_method_names_.insert(methods::kSecurePaymentConfirmation);
}

SecurePaymentConfirmationApp::~SecurePaymentConfirmationApp() = default;

void SecurePaymentConfirmationApp::InvokePaymentApp(
    base::WeakPtr<Delegate> delegate) {
  if (!authenticator_ || !spec_)
    return;

  DCHECK(spec_->IsInitialized());

  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->relying_party_id = effective_relying_party_identity_;
  options->timeout = request_->timeout.has_value()
                         ? request_->timeout.value()
                         : base::Minutes(kDefaultTimeoutMinutes);
  options->user_verification = device::UserVerificationRequirement::kRequired;
  std::vector<device::PublicKeyCredentialDescriptor> credentials;
  options->extensions =
      !request_->extensions
          ? blink::mojom::AuthenticationExtensionsClientInputs::New()
          : request_->extensions.Clone();

  if (base::FeatureList::IsEnabled(
          ::features::kSecurePaymentConfirmationDebug)) {
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

  options->challenge = request_->challenge;
  std::optional<std::vector<uint8_t>> browser_bound_public_key = std::nullopt;
#if BUILDFLAG(IS_ANDROID)
  if (passkey_browser_binder_) {
    std::vector<device::PublicKeyCredentialParams::CredentialInfo>
        credential_parameters = request_->browser_bound_pub_key_cred_params;
    if (credential_parameters.empty()) {
      credential_parameters =
          base::ToVector(kDefaultBrowserBoundKeyCredentialParameters);
    }
    auto on_get_browser_bound_key_callback = base::BindOnce(
        &SecurePaymentConfirmationApp::OnGetBrowserBoundKey,
        weak_ptr_factory_.GetWeakPtr(), delegate, std::move(options));
    if (web_contents()->GetBrowserContext()->IsOffTheRecord()) {
      passkey_browser_binder_->GetBoundKeyForPasskey(
          credential_id_, effective_relying_party_identity_,
          std::move(on_get_browser_bound_key_callback));
    } else {
      passkey_browser_binder_->GetOrCreateBoundKeyForPasskey(
          credential_id_, effective_relying_party_identity_,
          credential_parameters, std::move(on_get_browser_bound_key_callback));
    }
  } else {
    OnGetBrowserBoundKey(delegate, std::move(options),
                         /*browser_bound_key=*/nullptr);
  }
#else   // BUILDFLAG(IS_ANDROID))
  OnGetBrowserBoundKey(delegate, std::move(options),
                       /*browser_bound_key=*/nullptr);
#endif  // BUILDFLAG(IS_ANDROID))
}

bool SecurePaymentConfirmationApp::IsCompleteForPayment() const {
  return true;
}

bool SecurePaymentConfirmationApp::CanPreselect() const {
  return true;
}

std::u16string SecurePaymentConfirmationApp::GetMissingInfoLabel() const {
  NOTREACHED();
}

bool SecurePaymentConfirmationApp::HasEnrolledInstrument() const {
  // If the fallback feature is disabled, the factory should only create this
  // app if the authenticator and credentials were available. Therefore, this
  // function can always return true with the fallback feature disabled.
  return (authenticator_ && !credential_id_.empty()) ||
         !PaymentsExperimentalFeatures::IsEnabled(
             features::kSecurePaymentConfirmationFallback);
}

bool SecurePaymentConfirmationApp::NeedsInstallation() const {
  return false;
}

std::string SecurePaymentConfirmationApp::GetId() const {
  if (credential_id_.empty()) {
    CHECK(PaymentsExperimentalFeatures::IsEnabled(
        features::kSecurePaymentConfirmationFallback));
    // Since there is no credential_id_ in the fallback flow, we still must
    // return a non-empty app ID.
    return "spc";
  } else {
    return base::Base64Encode(credential_id_);
  }
}

std::u16string SecurePaymentConfirmationApp::GetLabel() const {
  return payment_instrument_label_;
}

std::u16string SecurePaymentConfirmationApp::GetSublabel() const {
  return std::u16string();
}

const SkBitmap* SecurePaymentConfirmationApp::icon_bitmap() const {
  return payment_instrument_icon_.get();
}

const SkBitmap* SecurePaymentConfirmationApp::issuer_bitmap() const {
  return issuer_icon_.get();
}

const SkBitmap* SecurePaymentConfirmationApp::network_bitmap() const {
  return network_icon_.get();
}

bool SecurePaymentConfirmationApp::IsValidForModifier(
    const std::string& method) const {
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

mojom::PaymentResponsePtr
SecurePaymentConfirmationApp::SetAppSpecificResponseFields(
    mojom::PaymentResponsePtr response) const {
  blink::mojom::GetAssertionAuthenticatorResponsePtr assertion_response =
      blink::mojom::GetAssertionAuthenticatorResponse::New(
          response_->info.Clone(), response_->authenticator_attachment,
          response_->signature, response_->user_handle,
          response_->extensions.Clone());
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys) &&
      assertion_response->extensions->payment.is_null()) {
    assertion_response->extensions->payment =
        blink::mojom::AuthenticationExtensionsPaymentResponse::New();
  }
  if (browser_bound_key_) {
    assertion_response->extensions->payment->browser_bound_signature =
        browser_bound_key_->Sign(response_->info->client_data_json);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  response->get_assertion_authenticator_response =
      std::move(assertion_response);
  return response;
}

void SecurePaymentConfirmationApp::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (content::RenderFrameHost::FromID(authenticator_frame_routing_id_) ==
      render_frame_host) {
    // The authenticator requires to be deleted before the render frame.
    authenticator_.reset();
  }
}

PasskeyBrowserBinder*
SecurePaymentConfirmationApp::GetPasskeyBrowserBinderForTesting() {
  return passkey_browser_binder_.get();
}

void SecurePaymentConfirmationApp::OnGetBrowserBoundKey(
    base::WeakPtr<Delegate> delegate,
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    std::unique_ptr<BrowserBoundKey> browser_bound_key) {
  browser_bound_key_ = std::move(browser_bound_key);
  std::optional<std::vector<uint8_t>> browser_bound_public_key = std::nullopt;
  if (browser_bound_key_) {
    browser_bound_public_key = browser_bound_key_->GetPublicKeyAsCoseKey();
  }
  // TODO(crbug.com/40225659): The 'showOptOut' flag status must also be signed
  // in the assertion, so that the verifier can check that the caller offered
  // the experience if desired.
  // TODO(crbug.com/333945861): The network and issuer information must also be
  // signed in the assertion, so that the verifier can check that the caller
  // passed the correct information.
  authenticator_->SetPaymentOptions(blink::mojom::PaymentOptions::New(
      spec_->GetTotal(/*selected_app=*/this)->amount.Clone(),
      request_->instrument.Clone(), request_->payee_name,
      request_->payee_origin, std::move(browser_bound_public_key)));

  authenticator_->GetAssertion(
      std::move(options),
      base::BindOnce(&SecurePaymentConfirmationApp::OnGetAssertion,
                     weak_ptr_factory_.GetWeakPtr(), delegate));
}

void SecurePaymentConfirmationApp::OnGetAssertion(
    base::WeakPtr<Delegate> delegate,
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  if (!delegate)
    return;

  if (status != blink::mojom::AuthenticatorStatus::SUCCESS || !response) {
    delegate->OnInstrumentDetailsError(
        errors::kWebAuthnOperationTimedOutOrNotAllowed);
    RecordSystemPromptResult(
        SecurePaymentConfirmationSystemPromptResult::kCanceled);
    return;
  }

  RecordSystemPromptResult(
      SecurePaymentConfirmationSystemPromptResult::kAccepted);

  response_ = std::move(response);

  delegate->OnInstrumentDetailsReady(methods::kSecurePaymentConfirmation, "{}",
                                     PayerData());
}

}  // namespace payments
