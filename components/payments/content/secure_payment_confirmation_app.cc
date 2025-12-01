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
#include "base/strings/utf_string_conversions.h"
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
#include "components/payments/core/secure_payment_confirmation_metrics.h"
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

const device::PublicKeyCredentialParams::CredentialInfo
    kDefaultBrowserBoundKeyCredentialParameters[] = {
        {device::CredentialType::kPublicKey,
         base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256)},
        {device::CredentialType::kPublicKey,
         base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kRs256)}};

}  // namespace

SecurePaymentConfirmationApp::SecurePaymentConfirmationApp(
    content::WebContents* web_contents_to_observe,
    const std::string& effective_relying_party_identity,
    const std::u16string& payment_instrument_label,
    const std::u16string& payment_instrument_details,
    std::unique_ptr<SkBitmap> payment_instrument_icon,
    std::vector<uint8_t> credential_id,
    std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder,
    bool device_supports_browser_bound_keys_in_hardware,
    const url::Origin& merchant_origin,
    base::WeakPtr<PaymentRequestSpec> spec,
    mojom::SecurePaymentConfirmationRequestPtr request,
    std::unique_ptr<webauthn::InternalAuthenticator> authenticator,
    std::vector<PaymentApp::PaymentEntityLogo> payment_entities_logos)
    : PaymentApp(/*icon_resource_id=*/0, PaymentApp::Type::INTERNAL),
      content::WebContentsObserver(web_contents_to_observe),
      authenticator_frame_routing_id_(
          authenticator ? authenticator->GetRenderFrameHost()->GetGlobalId()
                        : content::GlobalRenderFrameHostId()),
      effective_relying_party_identity_(effective_relying_party_identity),
      payment_instrument_label_(payment_instrument_label),
      payment_instrument_details_(payment_instrument_details),
      payment_instrument_icon_(std::move(payment_instrument_icon)),
      credential_id_(std::move(credential_id)),
      merchant_origin_(merchant_origin),
      spec_(spec),
      request_(std::move(request)),
      authenticator_(std::move(authenticator)),
      passkey_browser_binder_(std::move(passkey_browser_binder)),
      device_supports_browser_bound_keys_in_hardware_(
          device_supports_browser_bound_keys_in_hardware),
      payment_entities_logos_(std::move(payment_entities_logos)) {
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

  // Last used time is needed on platforms where the credentials cannot be
  // listed by platform APIs.
  std::optional<base::Time> last_used;
#if BUILDFLAG(IS_WIN)
  last_used = base::Time::NowFromSystemTime();
#endif

  if (passkey_browser_binder_) {
    std::vector<device::PublicKeyCredentialParams::CredentialInfo>
        credential_parameters = request_->browser_bound_pub_key_cred_params;
    if (credential_parameters.empty()) {
      credential_parameters =
          base::ToVector(kDefaultBrowserBoundKeyCredentialParameters);
    }
    if (web_contents()->GetBrowserContext()->IsOffTheRecord()) {
      passkey_browser_binder_->GetBoundKeyForPasskey(
          credential_id_, effective_relying_party_identity_,
          base::BindOnce(&SecurePaymentConfirmationApp::OnGetBrowserBoundKey,
                         weak_ptr_factory_.GetWeakPtr(), delegate,
                         std::move(options), /*is_new=*/false));
    } else {
      passkey_browser_binder_->GetOrCreateBoundKeyForPasskey(
          credential_id_, effective_relying_party_identity_,
          credential_parameters, std::move(last_used),
          base::BindOnce(&SecurePaymentConfirmationApp::OnGetBrowserBoundKey,
                         weak_ptr_factory_.GetWeakPtr(), delegate,
                         std::move(options)));
    }
  } else {
    OnGetBrowserBoundKey(delegate, std::move(options),
                         /*is_new=*/false, /*browser_bound_key=*/nullptr);
  }
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
         !(PaymentsExperimentalFeatures::IsEnabled(
               features::kSecurePaymentConfirmationFallback) ||
           base::FeatureList::IsEnabled(
               blink::features::kSecurePaymentConfirmationUxRefresh));
}

bool SecurePaymentConfirmationApp::NeedsInstallation() const {
  return false;
}

std::string SecurePaymentConfirmationApp::GetId() const {
  if (credential_id_.empty()) {
    CHECK(PaymentsExperimentalFeatures::IsEnabled(
              features::kSecurePaymentConfirmationFallback) ||
          base::FeatureList::IsEnabled(
              blink::features::kSecurePaymentConfirmationUxRefresh));
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
  return payment_instrument_details_;
}

const SkBitmap* SecurePaymentConfirmationApp::icon_bitmap() const {
  return payment_instrument_icon_.get();
}

std::vector<PaymentApp::PaymentEntityLogo*>
SecurePaymentConfirmationApp::GetPaymentEntitiesLogos() {
  // Filters logos with empty icons out from payment_entities_logos_. Once
  // network_bitmap() and issuer_bitmap() are no longer needed,
  // payment_entities_logos_ will no longer contain logos with empty icons, and
  // the filtering will not be required.
  //
  // TODO(crbug.com/428009834): Validate this before removing the filtering. If
  // a PaymentEntityLogo failed to download, the logo.icon would also be
  // nullptr and so filtering may still be necessary.
  std::vector<PaymentApp::PaymentEntityLogo*> filtered_logos;
  for (PaymentApp::PaymentEntityLogo& logo : payment_entities_logos_) {
    if (logo.icon != nullptr) {
      filtered_logos.push_back(&logo);
    }
  }
  return filtered_logos;
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
    bool is_new,
    std::unique_ptr<BrowserBoundKey> browser_bound_key) {
  browser_bound_key_ = std::move(browser_bound_key);
  std::optional<std::vector<uint8_t>> browser_bound_public_key = std::nullopt;
  if (browser_bound_key_) {
    browser_bound_public_key = browser_bound_key_->GetPublicKeyAsCoseKey();
    if (is_new) {
      RecordBrowserBoundKeyInclusion(
          SecurePaymentConfirmationBrowserBoundKeyInclusionResult::
              kIncludedNew);
    } else {
      RecordBrowserBoundKeyInclusion(
          SecurePaymentConfirmationBrowserBoundKeyInclusionResult::
              kIncludedExisting);

      // Update last used on platforms where the credentials cannot be listed by
      // platform APIs.
#if BUILDFLAG(IS_WIN)
      passkey_browser_binder_->UpdateKeyLastUsedToNow(
          credential_id_, effective_relying_party_identity_);
#endif
    }
  } else if (passkey_browser_binder_) {
    RecordBrowserBoundKeyInclusion(
        device_supports_browser_bound_keys_in_hardware_
            ? SecurePaymentConfirmationBrowserBoundKeyInclusionResult::
                  kNotIncludedWithDeviceHardware
            : SecurePaymentConfirmationBrowserBoundKeyInclusionResult::
                  kNotIncludedWithoutDeviceHardware);
  }
  // TODO(crbug.com/40225659): The 'showOptOut' flag status must also be signed
  // in the assertion, so that the verifier can check that the caller offered
  // the experience if desired.
  std::optional<std::vector<blink::mojom::ShownPaymentEntityLogoPtr>>
      payment_entities_logos;
  blink::mojom::PaymentCredentialInstrumentPtr instrument =
      request_->instrument.Clone();
  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationUxRefresh)) {
    payment_entities_logos.emplace();
    for (const PaymentApp::PaymentEntityLogo& logo : payment_entities_logos_) {
      // When the logo could not be download or decoded, then logo.icon is null.
      // In this case a ShownPaymentEntityLogo with an empty url is added, so
      // that clientData includes a placeholder when images failed to download.
      payment_entities_logos->push_back(
          blink::mojom::ShownPaymentEntityLogo::New(
              logo.icon ? logo.url : GURL::EmptyGURL(),
              base::UTF16ToUTF8(logo.label)));
    }
  } else {
    // If kSecurePaymentConfirmationUxRefresh is not enabled, then we did not
    // show the instrument details in the UI, and therefore we do not include
    // them in the clientData by setting to std::nullopt. Details should be
    // std::nullopt here because the dictionary field is already flag protected
    // on the render side; however, we also set it empty here on the
    // browser-side as well.
    instrument->details = std::nullopt;
  }
  authenticator_->SetPaymentOptions(blink::mojom::PaymentOptions::New(
      spec_->GetTotal(/*selected_app=*/this)->amount.Clone(),
      std::move(instrument), request_->payee_name, request_->payee_origin,
      /*payment_entities_logos=*/std::move(payment_entities_logos),
      std::move(browser_bound_public_key)));

  authenticator_->GetAssertion(
      std::move(options),
      base::BindOnce(&SecurePaymentConfirmationApp::OnGetAssertion,
                     weak_ptr_factory_.GetWeakPtr(), delegate));

  if (wait_for_get_bbk_for_tests_) {
    std::move(wait_for_get_bbk_for_tests_).Run();
  }
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
