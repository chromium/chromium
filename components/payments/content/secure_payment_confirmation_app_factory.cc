// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app_factory.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payments_experimental_features.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/payments/core/sizes.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webauthn_security_utils.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "components/payments/content/content_payment_request_delegate.h"
#endif

namespace payments {
namespace {

// Arbitrarily chosen limit of 1 hour. Keep in sync with
// secure_payment_confirmation_helper.cc.
constexpr int64_t kMaxTimeoutInMilliseconds = 1000 * 60 * 60;

// The maximum size of the payment instrument details string. Arbitrarily chosen
// while being much larger than any reasonable input.
constexpr size_t kMaxInstrumentDetailsSize = 4096;

// Determine whether an RP ID is a 'valid domain' as per the URL spec:
// https://url.spec.whatwg.org/#valid-domain
//
// TODO(crbug.com/40858925): This is a workaround to a lack of support for
// 'valid domain's in the //url code.
bool IsValidDomain(const std::string& rp_id) {
  // A valid domain, such as 'site.example', should be a URL host (and nothing
  // more of the URL!) that is not an IP address.
  GURL url("https://" + rp_id);
  return url.is_valid() && url.GetHost() == rp_id && !url.HostIsIPAddress();
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

  if (request->instrument->details.has_value()) {
    if (!base::IsStringUTF8(*request->instrument->details)) {
      *error_message = errors::kNonUtf8InstrumentDetailsString;
      return false;
    }

    if (request->instrument->details->empty()) {
      *error_message = errors::kEmptyInstrumentDetailsString;
      return false;
    }

    if (request->instrument->details->size() > kMaxInstrumentDetailsSize) {
      *error_message = errors::kTooLongInstrumentDetailsString;
      return false;
    }
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

  if (!request->payment_entities_logos.empty()) {
    for (const mojom::PaymentEntityLogoPtr& logo :
         request->payment_entities_logos) {
      if (logo.is_null()) {
        *error_message = errors::kNonNullPaymentEntityLogoRequired;
        return false;
      }

      if (!logo->url.is_valid()) {
        *error_message = errors::kValidLogoUrlRequired;
        return false;
      }
      if (!logo->url.SchemeIsHTTPOrHTTPS() &&
          !logo->url.SchemeIs(url::kDataScheme)) {
        *error_message = errors::kValidLogoUrlSchemeRequired;
        return false;
      }
      if (logo->label.empty()) {
        *error_message = errors::kLogoLabelRequired;
        return false;
      }
    }
  }

  return true;
}

struct IconInfo {
  GURL url;
  std::optional<int> request_id;
  SkBitmap icon;
};

// Handles the download of a given IconInfo; copying the downloaded bitmap into
// the IconInfo and notifying the BarrierClosure.
void DidDownloadIcon(IconInfo* icon_info,
                     base::OnceClosure done_closure,
                     int request_id,
                     int unused_http_status_code,
                     const GURL& unused_image_url,
                     const std::vector<SkBitmap>& bitmaps,
                     const std::vector<gfx::Size>& unused_sizes) {
  CHECK(icon_info);
  bool has_icon = icon_info->request_id.has_value() &&
                  icon_info->request_id.value() == request_id &&
                  !bitmaps.empty();
  icon_info->icon = has_icon ? bitmaps.front() : SkBitmap();
  std::move(done_closure).Run();
}

}  // namespace

// Holds information pertaining to a specific request to create an SPC payment
// app, i.e. for a single PaymentRequest object construction.
struct SecurePaymentConfirmationAppFactory::Request
    : public content::WebContentsObserver {
  Request(base::WeakPtr<PaymentAppFactory::Delegate> delegate,
          scoped_refptr<payments::WebPaymentsWebDataService> web_data_service,
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
  scoped_refptr<payments::WebPaymentsWebDataService> web_data_service;
  mojom::SecurePaymentConfirmationRequestPtr mojo_request;
  std::unique_ptr<webauthn::InternalAuthenticator> authenticator;
  IconInfo payment_instrument_icon_info;
  std::vector<IconInfo> payment_entities_logos_infos;
  std::unique_ptr<SecurePaymentConfirmationCredential> credential;
};

void SecurePaymentConfirmationAppFactory::
    OnIsUserVerifyingPlatformAuthenticatorAvailable(
        std::unique_ptr<Request> request,
        bool is_available) {
  if (!request->delegate || !request->delegate->GetWebContents()) {
    return;
  }

  if (!request->authenticator ||
      (!is_available && !base::FeatureList::IsEnabled(
                            ::features::kSecurePaymentConfirmationDebug))) {
#if BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(
            blink::features::kSecurePaymentConfirmationUxRefresh)) {
      // Skip getting matching credential IDs since the authenticator is not
      // available.
      OnRetrievedCredentials(
          std::move(request),
          std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>());
      return;
    }
#endif  // BUILDFLAG(IS_ANDROID)

    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  auto* request_ptr = request.get();
  credential_finder_->GetMatchingCredentials(
      request_ptr->mojo_request->credential_ids,
      request_ptr->mojo_request->rp_id,
      request_ptr->delegate->GetFrameSecurityOrigin(),
      request_ptr->authenticator.get(), request_ptr->web_data_service,
      base::BindOnce(
          &SecurePaymentConfirmationAppFactory::OnRetrievedCredentials,
          weak_ptr_factory_.GetWeakPtr(), std::move(request)));
}

SecurePaymentConfirmationAppFactory::SecurePaymentConfirmationAppFactory()
    : PaymentAppFactory(PaymentApp::Type::INTERNAL),
      credential_finder_(
          std::make_unique<SecurePaymentConfirmationCredentialFinder>()) {}
SecurePaymentConfirmationAppFactory::~SecurePaymentConfirmationAppFactory() =
    default;

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

      mojom::SecurePaymentConfirmationRequestPtr spc_request =
          method_data->secure_payment_confirmation.Clone();

      // Since only the first 2 icons are shown, remove the remaining logos.
      // Note that the SPC dialog on Chrome Android will CHECK() that no more
      // than 2 logos are provided.
      if (spc_request->payment_entities_logos.size() > 2) {
        spc_request->payment_entities_logos.erase(
            spc_request->payment_entities_logos.begin() + 2);
      }

      // Record if the user will be offered an opt-out experience. Technically
      // SPC has not been 'selected' yet in the conceptual PaymentRequest flow,
      // however we know that for SPC it must be the only payment method offered
      // so we are safe to record this now.
      if (spc_request->show_opt_out) {
        delegate->SetOptOutOffered();
      }

      std::unique_ptr<webauthn::InternalAuthenticator> authenticator =
          delegate->CreateInternalAuthenticator();
      if (!authenticator) {
        delegate->OnDoneCreatingPaymentApps();
        return;
      }
      scoped_refptr<payments::WebPaymentsWebDataService> web_data_service =
          delegate->GetWebPaymentsWebDataService();
      if (!web_data_service) {
        delegate->OnDoneCreatingPaymentApps();
        return;
      }
      auto* authenticator_pointer = authenticator.get();
      authenticator_pointer->IsUserVerifyingPlatformAuthenticatorAvailable(
          base::BindOnce(&SecurePaymentConfirmationAppFactory::
                             OnIsUserVerifyingPlatformAuthenticatorAvailable,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::make_unique<Request>(delegate, web_data_service,
                                                   std::move(spc_request),
                                                   std::move(authenticator))));
      return;
    }
  }

  delegate->OnDoneCreatingPaymentApps();
}

void SecurePaymentConfirmationAppFactory::SetBrowserBoundKeyStoreForTesting(
    scoped_refptr<BrowserBoundKeyStore> key_store) {
  browser_bound_key_store_for_testing_ = std::move(key_store);
}

void SecurePaymentConfirmationAppFactory::SetCredentialFinderForTesting(
    std::unique_ptr<SecurePaymentConfirmationCredentialFinder>
        credential_finder) {
  credential_finder_ = std::move(credential_finder);
}

void SecurePaymentConfirmationAppFactory::OnRetrievedCredentials(
    std::unique_ptr<Request> request,
    std::optional<
        std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
        credentials) {
  if (!request->delegate || !request->delegate->GetWebContents()) {
    return;
  }

  if (!credentials.has_value()) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  // Regardless of whether any credentials match, canMakePayment() and
  // hasEnrolledInstrument() should return true for SPC when a user-verifying
  // platform authenticator device is available.
  request->delegate->SetCanMakePaymentEvenWithoutApps();

  // For the pilot phase, arbitrarily use the first matching credential.
  // TODO(crbug.com/40142088): Handle multiple credentials.
  if (!credentials->empty()) {
    request->credential = std::move(credentials->front());
  }

  // Download the icons for the payment instrument icon and the payment entity
  // logos. These download URLs were passed into the PaymentRequest API. If
  // given icon URL wasn't specified, then DownloadImageInFrame will simply
  // return an empty set of bitmaps.
  //
  // Perform these downloads regardless of whether there is a matching
  // credential, so that the hosting server(s) cannot detect presence of the
  // credential on file.
  auto* request_ptr = request.get();

  request_ptr->payment_instrument_icon_info = {
      .url = request_ptr->mojo_request->instrument->icon};
  for (const mojom::PaymentEntityLogoPtr& logo :
       request_ptr->mojo_request->payment_entities_logos) {
    request_ptr->payment_entities_logos_infos.push_back({.url = logo->url});
  }

  auto barrier_closure = base::BarrierClosure(
      // The payment instrument icon download, plus any payment entity logos.
      1 + request_ptr->payment_entities_logos_infos.size(),
      base::BindOnce(&SecurePaymentConfirmationAppFactory::DidDownloadAllIcons,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)));

  gfx::Size preferred_size(kSecurePaymentConfirmationIconMaximumWidthPx,
                           kSecurePaymentConfirmationIconHeightPx);

  request_ptr->payment_instrument_icon_info.request_id =
      request_ptr->web_contents()->DownloadImageInFrame(
          request_ptr->delegate->GetInitiatorRenderFrameHostId(),
          request_ptr->payment_instrument_icon_info.url,  // source URL
          false,                                          // is_favicon
          preferred_size,
          0,      // no max size
          false,  // normal cache policy (a.k.a. do not bypass cache)
          base::BindOnce(&DidDownloadIcon,
                         &request_ptr->payment_instrument_icon_info,
                         barrier_closure));

  for (IconInfo& info : request_ptr->payment_entities_logos_infos) {
    if (info.url.is_empty()) {
      // This IconInfo is a placeholder value. No download is necessary.
      barrier_closure.Run();
    } else {
      info.request_id = request_ptr->web_contents()->DownloadImageInFrame(
          request_ptr->delegate->GetInitiatorRenderFrameHostId(),
          info.url,  // source URL
          false,     // is_favicon
          preferred_size,
          0,      // no max size
          false,  // normal cache policy (a.k.a. do not bypass cache)
          base::BindOnce(&DidDownloadIcon, &info, barrier_closure));
    }
  }
}

void SecurePaymentConfirmationAppFactory::DidDownloadAllIcons(
    std::unique_ptr<Request> request) {
  DCHECK(request);
  if (!request->delegate || !request->web_contents())
    return;

  SkBitmap payment_instrument_icon = request->payment_instrument_icon_info.icon;
  if (payment_instrument_icon.drawsNothing()) {
    // If the option iconMustBeShown is true, which it is by default, in the
    // case of a failed instrument icon download/decode, we reject the show()
    // promise without showing any user UX. To avoid a privacy leak here, we
    // MUST do this check ahead of checking whether any credential matched, as
    // otherwise an attacker could deliberately pass an invalid icon and do a
    // timing attack to see if a credential matches.
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

  bool skip_spc_app_creation = !request->delegate->GetSpec();
  bool has_authenticator_and_credential =
      request->authenticator && request->credential;
#if BUILDFLAG(IS_ANDROID)
  skip_spc_app_creation =
      skip_spc_app_creation ||
      (!has_authenticator_and_credential &&
       !PaymentsExperimentalFeatures::IsEnabled(
           features::kSecurePaymentConfirmationFallback) &&
       !base::FeatureList::IsEnabled(
           blink::features::kSecurePaymentConfirmationUxRefresh));
#else
  skip_spc_app_creation =
      skip_spc_app_creation || !has_authenticator_and_credential;
#endif  // BUILDFLAG(IS_ANDROID)
  if (skip_spc_app_creation) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  std::u16string payment_instrument_label =
      base::UTF8ToUTF16(request->mojo_request->instrument->display_name);
  std::u16string payment_instrument_details = base::UTF8ToUTF16(
      request->mojo_request->instrument->details.value_or(""));

  CHECK_EQ(request->mojo_request->payment_entities_logos.size(),
           request->payment_entities_logos_infos.size());
  std::vector<SecurePaymentConfirmationApp::PaymentEntityLogo>
      payment_entities_logos;
  for (size_t i = 0; i < request->payment_entities_logos_infos.size(); i++) {
    SkBitmap& bitmap = request->payment_entities_logos_infos[i].icon;
    payment_entities_logos.emplace_back(
        base::UTF8ToUTF16(
            request->mojo_request->payment_entities_logos[i]->label),
        bitmap.drawsNothing() ? nullptr : std::make_unique<SkBitmap>(bitmap),
        std::move(request->mojo_request->payment_entities_logos[i]->url));
  }

  if (!request->authenticator || !request->credential) {
    CHECK(PaymentsExperimentalFeatures::IsEnabled(
              features::kSecurePaymentConfirmationFallback) ||
          base::FeatureList::IsEnabled(
              blink::features::kSecurePaymentConfirmationUxRefresh));
    // In the case of no authenticator or credentials, we still create the
    // SecurePaymentConfirmationApp, which holds the information to be shown
    // in the fallback UX.
    request->delegate->OnPaymentAppCreated(
        std::make_unique<SecurePaymentConfirmationApp>(
            request->web_contents(),
            /*effective_relying_party_identity=*/std::string(),
            payment_instrument_label, payment_instrument_details,
            std::make_unique<SkBitmap>(payment_instrument_icon),
            /*credential_id=*/std::vector<uint8_t>(),
            /*passkey_browser_binder=*/nullptr,
            /*device_supports_browser_bound_keys_in_hardware=*/false,
            url::Origin::Create(request->delegate->GetTopOrigin()),
            request->delegate->GetSpec()->AsWeakPtr(),
            std::move(request->mojo_request), /*authenticator=*/nullptr,
            std::move(payment_entities_logos)));
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder;
  bool device_supports_browser_bound_keys_in_hardware = false;
#if !BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    scoped_refptr key_store =
        browser_bound_key_store_for_testing_
            ? std::move(browser_bound_key_store_for_testing_)
            : GetBrowserBoundKeyStoreInstance(BrowserBoundKeyStore::Config{
#if BUILDFLAG(IS_MAC)
                  .keychain_access_group =
                      request->delegate->GetPaymentRequestDelegate()
                          ->GetSecurePaymentConfirmationKeychainAccessGroup()
#endif  // BUILDFLAG(IS_MAC)
              });
    device_supports_browser_bound_keys_in_hardware =
        key_store->GetDeviceSupportsHardwareKeys();
    passkey_browser_binder = std::make_unique<PasskeyBrowserBinder>(
        std::move(key_store), request->web_data_service);
  }
#endif  // !BUILDFLAG(IS_IOS)

  request->delegate->OnPaymentAppCreated(
      std::make_unique<SecurePaymentConfirmationApp>(
          request->web_contents(), request->credential->relying_party_id,
          payment_instrument_label, payment_instrument_details,
          std::make_unique<SkBitmap>(payment_instrument_icon),
          std::move(request->credential->credential_id),
          std::move(passkey_browser_binder),
          device_supports_browser_bound_keys_in_hardware,
          url::Origin::Create(request->delegate->GetTopOrigin()),
          request->delegate->GetSpec()->AsWeakPtr(),
          std::move(request->mojo_request), std::move(request->authenticator),
          std::move(payment_entities_logos)));

  request->delegate->OnDoneCreatingPaymentApps();
}

}  // namespace payments
