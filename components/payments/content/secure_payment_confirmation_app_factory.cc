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
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payments_experimental_features.h"
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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

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

  if (!base::IsStringUTF8(request->instrument->details)) {
    *error_message = errors::kNonUtf8InstrumentDetailsString;
    return false;
  }

  if (request->instrument->details.size() > kMaxInstrumentDetailsSize) {
    *error_message = errors::kTooLongInstrumentDetailsString;
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

  if (request->network_info) {
    if (request->network_info->name.empty()) {
      *error_message = errors::kNetworkNameRequired;
      return false;
    }
    if (!request->network_info->icon.is_valid()) {
      *error_message = errors::kValidNetworkIconRequired;
      return false;
    }
  }

  if (request->issuer_info) {
    if (request->issuer_info->name.empty()) {
      *error_message = errors::kIssuerNameRequired;
      return false;
    }
    if (!request->issuer_info->icon.is_valid()) {
      *error_message = errors::kValidIssuerIconRequired;
      return false;
    }
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

// Determine if a given origin that is calling SPC with a given RP ID requires
// the credentials to be third-party enabled (i.e., the calling party is not the
// RP ID).
bool RequiresThirdPartyPaymentBit(const url::Origin& caller_origin,
                                  const std::string& relying_party_id) {
  return !content::OriginIsAllowedToClaimRelyingPartyId(relying_party_id,
                                                        caller_origin);
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
  IconInfo payment_instrument_icon_info;
  std::vector<IconInfo> payment_entities_logos_infos;
  std::unique_ptr<SecurePaymentConfirmationCredential> credential;
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
  std::ranges::for_each(requests_, [&](const auto& pair) {
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

      // We currently support two ways to specify logos to be shown on the UX:
      // the old (experimental) network_info/issuer_info fields, and the new
      // payment_entities_logos field. Both are flag-guarded, and only one flow
      // is supported at a time, so to simplify the rest of the logic we
      // consolidate issuer_info/network_info (if set) into
      // payment_entities_logos.
      //
      // If both flags are turned on then payment_entities_logos will 'win' and
      // network_info and issuer_info will be ignored.
      //
      // TODO(crbug.com/417683819): Remove this code once network_info and
      // issuer_info have been fully deprecated and removed.
      mojom::SecurePaymentConfirmationRequestPtr spc_request =
          method_data->secure_payment_confirmation.Clone();
      if (!base::FeatureList::IsEnabled(
              blink::features::kSecurePaymentConfirmationUxRefresh) &&
          (spc_request->network_info || spc_request->issuer_info)) {
        spc_request->payment_entities_logos.clear();

        // We encode the network and issuer info as network first, issuer
        // second. If network was not provided, we insert a placeholder so that
        // later code can properly map the order back.
        if (spc_request->network_info) {
          spc_request->payment_entities_logos.emplace_back(
              mojom::PaymentEntityLogo::New(
                  /*url=*/spc_request->network_info->icon,
                  /*label=*/spc_request->network_info->name));
        } else {
          spc_request->payment_entities_logos.emplace_back(
              mojom::PaymentEntityLogo::New(/*url=*/GURL(), /*label=*/""));
        }

        if (spc_request->issuer_info) {
          spc_request->payment_entities_logos.emplace_back(
              mojom::PaymentEntityLogo::New(
                  /*url=*/spc_request->issuer_info->icon,
                  /*label=*/spc_request->issuer_info->name));
        }
      }

      // Only spc_request->payment_entities_logos should be used from here out.
      spc_request->network_info = nullptr;
      spc_request->issuer_info = nullptr;

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
                         std::make_unique<Request>(delegate, web_data_service,
                                                   std::move(spc_request),
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

  if (result && result->GetType() == SECURE_PAYMENT_CONFIRMATION) {
    std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>
        credentials = static_cast<WDResult<std::vector<
            std::unique_ptr<SecurePaymentConfirmationCredential>>>*>(
                          result.get())
                          ->GetValue();
    OnRetrievedCredentials(std::move(request), std::move(credentials));
  } else {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }
}

#if BUILDFLAG(IS_ANDROID)
void SecurePaymentConfirmationAppFactory::SetBrowserBoundKeyStoreForTesting(
    scoped_refptr<BrowserBoundKeyStore> key_store) {
  browser_bound_key_store_for_testing_ = std::move(key_store);
}
#endif  // BUILDFLAG(IS_ANDROID)

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
  // For the pilot phase, arbitrarily use the first matching credential.
  // TODO(crbug.com/40142088): Handle multiple credentials.
  if (!credentials.empty())
    request->credential = std::move(credentials.front());

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

  if (!request->delegate->GetSpec() ||
      ((!request->authenticator || !request->credential) &&
       !(PaymentsExperimentalFeatures::IsEnabled(
             features::kSecurePaymentConfirmationFallback) ||
         base::FeatureList::IsEnabled(
             blink::features::kSecurePaymentConfirmationUxRefresh)))) {
    request->delegate->OnDoneCreatingPaymentApps();
    return;
  }

  std::u16string payment_instrument_label =
      base::UTF8ToUTF16(request->mojo_request->instrument->display_name);
  std::u16string payment_instrument_details =
      base::UTF8ToUTF16(request->mojo_request->instrument->details);

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
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    scoped_refptr key_store =
        browser_bound_key_store_for_testing_
            ? std::move(browser_bound_key_store_for_testing_)
            : GetBrowserBoundKeyStoreInstance();
    device_supports_browser_bound_keys_in_hardware =
        key_store->GetDeviceSupportsHardwareKeys();
    passkey_browser_binder = std::make_unique<PasskeyBrowserBinder>(
        std::move(key_store), request->web_data_service);
  }
#endif  // BUILDFLAG(IS_ANDROID)

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
