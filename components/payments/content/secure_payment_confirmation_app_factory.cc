// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app_factory.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/payments/core/sizes.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webauthn_security_utils.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

// Arbitrarily chosen limit of 1 hour. Keep in sync with
// secure_payment_confirmation_helper.cc.
constexpr int64_t kMaxTimeoutInMilliseconds = 1000 * 60 * 60;

// Determine whether an RP ID is a 'valid domain' as per the URL spec:
// https://url.spec.whatwg.org/#valid-domain
//
// TODO(crbug.com/1354209): This is a workaround to a lack of support for 'valid
// domain's in the //url code.
bool IsValidDomain(const std::string& rp_id) {
  // A valid domain, such as 'site.example', should be a URL host (and nothing
  // more of the URL!) that is not an IP address.
  GURL url("https://" + rp_id);
  return url.is_valid() && url.host() == rp_id && !url.HostIsIPAddress();
}

bool IsValid(const mojom::SecurePaymentConfirmationRequestPtr& request,
             std::string* error_message) {
  // `request` can be null when the feature is disabled in Blink.
  if (!request)
    return false;

  // The remaining steps in this method check that the renderer has sent us a
  // valid SecurePaymentConfirmationRequest, to guard against a compromised
  // renderer.

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

  if (!IsValidDomain(request->rp_id)) {
    *error_message = errors::kRpIdRequired;
    return false;
  }

  if ((!request->payee_origin.has_value() &&
       !request->payee_name.has_value()) ||
      (request->payee_name.has_value() && request->payee_name->empty())) {
    *error_message = errors::kPayeeOriginOrPayeeNameRequired;
    return false;
  }

  if (request->payee_origin.has_value() &&
      request->payee_origin->scheme() != url::kHttpsScheme) {
    *error_message = errors::kPayeeOriginMustBeHttps;
    return false;
  }

  return true;
}

// Determine if a given origin that is calling SPC with a given RP ID requires
// the credentials to be third-party enabled (i.e., the calling party is not the
// RP ID).
bool RequiresThirdPartyPaymentBit(const url::Origin& caller_origin,
                                  const std::string& relying_party_id) {
  return !content::OriginIsAllowedToClaimRelyingPartyId(relying_party_id,
                                                        caller_origin);
}

}  // namespace

struct SecurePaymentConfirmationAppFactory::Request
    : public content::WebContentsObserver {
  Request(
      base::WeakPtr<PaymentAppFactory::Delegate> delegate,
      scoped_refptr<payments::PaymentManifestWebDataService> web_data_service,
      mojom::SecurePaymentConfirmationRequestPtr mojo_request,
      std::unique_ptr<webauthn::InternalAuthenticator> authenticator)
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
  std::unique_ptr<webauthn::InternalAuthenticator> authenticator;
  absl::optional<int> pending_icon_download_request_id;
};

void SecurePaymentConfirmationAppFactory::
    OnIsUserVerifyingPlatformAuthenticatorAvailable(
        std::unique_ptr<Request> request,
        bool is_available) {
  if (!request->delegate || !request->delegate->GetWebContents())
    return;

  if (!request->authenticator ||
      (!is_available && !base::FeatureList::IsEnabled(
                            ::features::kSecurePaymentConfirmationDebug))) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  // If we are relying on underlying credential-store level support for SPC, but
  // it isn't available, ensure that canMakePayment() will return false by
  // returning early here.
  //
  // This helps websites avoid a failure scenario when SPC appears to be
  // available, but in practice it is non-functional due to lack of platform
  // support.
  if (base::FeatureList::IsEnabled(
          features::kSecurePaymentConfirmationUseCredentialStoreAPIs) &&
      !request->authenticator->IsGetMatchingCredentialIdsSupported()) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  // Regardless of whether any credentials match, canMakePayment() and
  // hasEnrolledInstrument() should return true for SPC when a user-verifying
  // platform authenticator device is available.
  request->delegate->SetCanMakePaymentEvenWithoutApps();

  // If we have credential-store level support for SPC, we can query the store
  // directly. Otherwise, we have to rely on the user profile database.
  //
  // Currently, credential store APIs are only available on Android.
  if (base::FeatureList::IsEnabled(
          features::kSecurePaymentConfirmationUseCredentialStoreAPIs)) {
    std::string relying_party_id = request->mojo_request->rp_id;
    const bool require_third_party_payment_bit = RequiresThirdPartyPaymentBit(
        request->delegate->GetFrameSecurityOrigin(), relying_party_id);

    auto* request_ptr = request.get();
    request_ptr->authenticator->GetMatchingCredentialIds(
        std::move(request_ptr->mojo_request->rp_id),
        std::move(request_ptr->mojo_request->credential_ids),
        require_third_party_payment_bit,
        base::BindOnce(&SecurePaymentConfirmationAppFactory::
                           OnGetMatchingCredentialIdsFromStore,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request),
                       std::move(relying_party_id)));
  } else {
    WebDataServiceBase::Handle handle =
        request->web_data_service->GetSecurePaymentConfirmationCredentials(
            std::move(request->mojo_request->credential_ids),
            std::move(request->mojo_request->rp_id), this);
    requests_[handle] = std::move(request);
  }
}

SecurePaymentConfirmationAppFactory::SecurePaymentConfirmationAppFactory()
    : PaymentAppFactory(PaymentApp::Type::INTERNAL) {}

SecurePaymentConfirmationAppFactory::~SecurePaymentConfirmationAppFactory() {
  base::ranges::for_each(requests_, [&](const auto& pair) {
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

      // Record if the user will be offered an opt-out experience. Technically
      // SPC has not been 'selected' yet in the conceptual PaymentRequest flow,
      // however we know that for SPC it must be the only payment method offered
      // so we are safe to record this now.
      if (method_data->secure_payment_confirmation->show_opt_out) {
        delegate->SetOptOutOffered();
      }

      std::unique_ptr<webauthn::InternalAuthenticator> authenticator =
          delegate->CreateInternalAuthenticator();
      if (!authenticator) {
        delegate->OnDoneCreatingPaymentApps();
        return;
      }
      scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
          delegate->GetPaymentManifestWebDataService();
      if (!web_data_service) {
        delegate->OnDoneCreatingPaymentApps();
        return;
      }
      auto* authenticator_pointer = authenticator.get();
      authenticator_pointer->IsUserVerifyingPlatformAuthenticatorAvailable(
          base::BindOnce(&SecurePaymentConfirmationAppFactory::
                             OnIsUserVerifyingPlatformAuthenticatorAvailable,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::make_unique<Request>(
                             delegate, web_data_service,
                             method_data->secure_payment_confirmation.Clone(),
                             std::move(authenticator))));
      return;
    }
  }

  delegate->OnDoneCreatingPaymentApps();
}

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

  std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>
      credentials = static_cast<WDResult<
          std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>*>(
                        result.get())
                        ->GetValue();

  OnRetrievedCredentials(std::move(request), std::move(credentials));
}

void SecurePaymentConfirmationAppFactory::OnGetMatchingCredentialIdsFromStore(
    std::unique_ptr<Request> request,
    std::string relying_party_id,
    std::vector<std::vector<uint8_t>> matching_credentials) {
  std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>> credentials;
  for (std::vector<uint8_t>& credential_id : matching_credentials) {
    credentials.emplace_back(
        std::make_unique<SecurePaymentConfirmationCredential>(
            std::move(credential_id), relying_party_id,
            /*user_id=*/std::vector<uint8_t>()));
  }
  OnRetrievedCredentials(std::move(request), std::move(credentials));
}

void SecurePaymentConfirmationAppFactory::OnRetrievedCredentials(
    std::unique_ptr<Request> request,
    std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>
        credentials) {
  std::unique_ptr<SecurePaymentConfirmationCredential> credential;

  // For the pilot phase, arbitrarily use the first matching credential.
  // TODO(https://crbug.com/1110320): Handle multiple credentials.
  if (!credentials.empty())
    credential = std::move(credentials.front());

  // Download the icon for the payment instrument. The download URL was passed
  // into the PaymentRequest API.
  //
  // Perform this download regardless of whether there is a matching
  // credential, so that the server that hosts the image cannot detect presence
  // of the credential on file.
  auto* request_ptr = request.get();
  gfx::Size preferred_size(
      kSecurePaymentConfirmationInstrumentIconMaximumWidthPx,
      kSecurePaymentConfirmationInstrumentIconHeightPx);
  request_ptr->pending_icon_download_request_id =
      request_ptr->web_contents()->DownloadImageInFrame(
          request_ptr->delegate->GetInitiatorRenderFrameHostId(),
          request_ptr->mojo_request->instrument->icon,  // source URL
          false,                                        // is_favicon
          preferred_size,
          0,      // no max size
          false,  // normal cache policy (a.k.a. do not bypass cache)
          base::BindOnce(&SecurePaymentConfirmationAppFactory::DidDownloadIcon,
                         weak_ptr_factory_.GetWeakPtr(), std::move(credential),
                         std::move(request)));
}

void SecurePaymentConfirmationAppFactory::OnAppIcon(
    std::unique_ptr<SecurePaymentConfirmationCredential> credential,
    std::unique_ptr<Request> request,
    const SkBitmap& icon) {
  DCHECK(request);
  if (!request->delegate || !request->web_contents())
    return;

  if (icon.drawsNothing()) {
    // If the option iconMustBeShown is true, which it is by default, in the
    // case of a failed icon download/decode, we reject the show() promise
    // without showing any user UX. To avoid a privacy leak here, we MUST do
    // this check ahead of checking whether any credential matched, as otherwise
    // an attacker could deliberately pass an invalid icon and do a timing
    // attack to see if a credential matches.
    if (request->mojo_request->instrument->iconMustBeShown) {
      request->delegate->OnPaymentAppCreationError(
          errors::kInvalidIcon, AppCreationFailureReason::ICON_DOWNLOAD_FAILED);
      request->delegate->OnDoneCreatingPaymentApps();
      return;
    }

    // Otherwise, we use a default icon and clear the icon URL to indicate this
    // in the output.
    request->mojo_request->instrument->icon = GURL();
  }

  if (!request->delegate->GetSpec() || !request->authenticator || !credential) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  std::u16string label =
      base::UTF8ToUTF16(request->mojo_request->instrument->display_name);

  request->delegate->OnPaymentAppCreated(
      std::make_unique<SecurePaymentConfirmationApp>(
          request->web_contents(), credential->relying_party_id,
          std::make_unique<SkBitmap>(icon), label,
          std::move(credential->credential_id),
          url::Origin::Create(request->delegate->GetTopOrigin()),
          request->delegate->GetSpec()->AsWeakPtr(),
          std::move(request->mojo_request), std::move(request->authenticator)));

  request->delegate->OnDoneCreatingPaymentApps();
}

void SecurePaymentConfirmationAppFactory::DidDownloadIcon(
    std::unique_ptr<SecurePaymentConfirmationCredential> credential,
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
  OnAppIcon(std::move(credential), std::move(request),
            has_icon ? bitmaps.front() : SkBitmap());
}

}  // namespace payments
