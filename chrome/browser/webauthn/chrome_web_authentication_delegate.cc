// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_web_authentication_delegate.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/unexportable_key_utils.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/webauthn/core/browser/passkey_change_quota_tracker.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "content/public/browser/web_contents.h"
#include "crypto/unexportable_key.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/features.h"
#include "device/fido/mac/credential_metadata.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/webauthn/webauthn_request_registrar.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#endif

namespace {

void LogSignalUnknownCredential(
    ChromeWebAuthenticationDelegate::SignalUnknownCredentialResult result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey", result);
}

void LogSignalAllAcceptedCredentials(
    ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult
        result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
      result);
}

void LogSignalCurrentUserDetailsUpdated(
    ChromeWebAuthenticationDelegate::SignalCurrentUserDetailsResult result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.SignalCurrentUserDetailsUpdatedGPMPasskey", result);
}

// Returns true iff |relying_party_id| is listed in the
// SecurityKeyPermitAttestation policy.
bool IsWebAuthnRPIDListedInSecurityKeyPermitAttestationPolicy(
    content::BrowserContext* browser_context,
    const std::string& relying_party_id) {
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  const PrefService* prefs = profile->GetPrefs();
  const base::Value::List& permit_attestation =
      prefs->GetList(prefs::kSecurityKeyPermitAttestation);
  return base::Contains(permit_attestation, relying_party_id);
}

bool IsOriginListedInEnterpriseAttestationSwitch(
    const url::Origin& caller_origin) {
  std::string cmdline_origins =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          webauthn::switches::kPermitEnterpriseAttestationOriginList);
  std::vector<std::string_view> origin_strings = base::SplitStringPiece(
      cmdline_origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::ranges::any_of(
      origin_strings, [&caller_origin](std::string_view origin_string) {
        return url::Origin::Create(GURL(origin_string)) == caller_origin;
      });
}

// Returns true if |extension| is allowed to create and assert |rp_id|.
bool ExtensionCanAssertRpId(const extensions::Extension& extension,
                            const std::string& rp_id) {
  // Extensions are always allowed to assert their own extension identifier.
  // This has special handling in
  // ChromeWebAuthenticationDelegate::MaybeGetRelyingPartyIdOverride, the
  // RP ID will be prefixed with the extension scheme to isolate it from web
  // origins.
  if (extension.id() == rp_id) {
    return true;
  }

  // Extensions may not claim eTLDs as RP IDs, even if WebAuthn does not
  // forbid origins from doing so if they are eTLDs themselves.
  if (!net::registry_controlled_domains::HostHasRegistryControlledDomain(
          base::TrimString(rp_id, ".", base::TrimPositions::TRIM_TRAILING),
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES) &&
      !net::HostStringIsLocalhost(rp_id)) {
    return false;
  }

  // An extension should be able to assert a WebAuthn RP ID if it has host
  // permissions over any host that can assert that RP ID. This code duplicates
  // some of the logic on content/public/browser/webauthn_security_utils.h.
  // https://w3c.github.io/webauthn/#relying-party-identifier
  for (const URLPattern& pattern :
       extension.permissions_data()->active_permissions().explicit_hosts()) {
    // Only https hosts and localhost are allowed to assert RP IDs. By design,
    // this means extensions cannot claim RP IDs for other extensions.
    if (!pattern.MatchesScheme(url::kHttpsScheme) &&
        !(pattern.MatchesScheme(url::kHttpScheme) &&
          net::HostStringIsLocalhost(pattern.host()))) {
      continue;
    }
    // IP hosts are not allowed to assert RP IDs.
    if (url::HostIsIPAddress(pattern.host())) {
      continue;
    }
    // If the pattern matches the RP ID, then it is allowed to assert it.
    // Pattern                   RP ID                     Allowed?
    // *.com                     example.com               Yes
    // example.com               example.com               Yes
    // *.example.com             subdomain.example.com     Yes
    if (pattern.MatchesHost(rp_id)) {
      return true;
    }
    // If the pattern matchens any valid subdomain of the RP ID, then it is
    // allowed to assert it, since subdomains can assert parent components up to
    // eTLD+1 on WebAuthn.
    // Pattern                   RP ID                     Allowed?
    // subdomain.example.com     example.com               Yes
    // *.subdomain.example.com   example.com               Yes
    // example.com               subdomain.example.com     No
    if (url::DomainIs(pattern.host(), rp_id)) {
      return true;
    }
  }
  return false;
}

void DeleteUnacceptedPasskeys(
    content::WebContents* web_contents,
    const std::string& relying_party_id,
    const std::vector<uint8_t>& user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids) {
  webauthn::PasskeyModel* passkey_store =
      PasskeyModelFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  bool is_passkey_deleted = false;
  for (const auto& passkey :
       passkey_store->GetPasskeysForRelyingPartyId(relying_party_id)) {
    if (std::vector<uint8_t>(passkey.user_id().begin(),
                             passkey.user_id().end()) == user_id &&
        !base::Contains(all_accepted_credentials_ids,
                        std::vector<uint8_t>(passkey.credential_id().begin(),
                                             passkey.credential_id().end()))) {
      passkey_store->DeletePasskey(passkey.credential_id(), FROM_HERE);
      is_passkey_deleted = true;
    }
  }
  if (is_passkey_deleted) {
    PasswordsClientUIDelegate* manage_passwords_ui_controller =
        PasswordsClientUIDelegateFromWebContents(web_contents);
    if (manage_passwords_ui_controller) {
      manage_passwords_ui_controller->OnPasskeyNotAccepted(relying_party_id);
    }
  }
  LogSignalAllAcceptedCredentials(
      is_passkey_deleted
          ? ChromeWebAuthenticationDelegate::
                SignalAllAcceptedCredentialsResult::kPasskeyRemoved
          : ChromeWebAuthenticationDelegate::
                SignalAllAcceptedCredentialsResult::kNoPasskeyChanged);
}

void HideAndRestorePasskeys(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::string& relying_party_id,
    const std::vector<uint8_t>& user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids) {
  webauthn::PasskeyChangeQuotaTracker* quota_tracker =
      webauthn::PasskeyChangeQuotaTracker::GetInstance();
  if (!quota_tracker->CanMakeChange(origin)) {
    LogSignalAllAcceptedCredentials(
        ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
            kQuotaExceeded);
    FIDO_LOG(ERROR) << "Dropping all accepted credentials request from "
                    << origin << ": quota exceeded.";
    return;
  }
  webauthn::PasskeyModel* passkey_store =
      PasskeyModelFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_store->GetPasskeysForRelyingPartyId(relying_party_id);
  const auto passkey_it =
      std::ranges::find_if(passkeys, [&user_id](const auto& passkey) {
        return std::vector<uint8_t>(passkey.user_id().begin(),
                                    passkey.user_id().end()) == user_id;
      });
  if (passkey_it == passkeys.end()) {
    LogSignalAllAcceptedCredentials(
        ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
            kNoPasskeyChanged);
    return;
  }
  bool passkey_in_list =
      base::Contains(all_accepted_credentials_ids,
                     std::vector<uint8_t>(passkey_it->credential_id().begin(),
                                          passkey_it->credential_id().end()));
  if ((passkey_in_list && !passkey_it->hidden()) ||
      (!passkey_in_list && passkey_it->hidden())) {
    LogSignalAllAcceptedCredentials(
        ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
            kNoPasskeyChanged);
    return;
  }

  if (passkey_in_list) {
    passkey_store->UnhidePasskey(passkey_it->credential_id());
  } else {
    passkey_store->HidePasskey(passkey_it->credential_id(), base::Time::Now());
  }
  quota_tracker->TrackChange(origin);
  LogSignalAllAcceptedCredentials(
      passkey_in_list ? ChromeWebAuthenticationDelegate::
                            SignalAllAcceptedCredentialsResult::kPasskeyRestored
                      : ChromeWebAuthenticationDelegate::
                            SignalAllAcceptedCredentialsResult::kPasskeyHidden);
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents);
  if (passkey_in_list && manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnPasskeyUpdated(relying_party_id);
  }
  if (!passkey_in_list && manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnPasskeyNotAccepted(relying_party_id);
  }
}

}  // namespace

ChromeWebAuthenticationDelegate::ChromeWebAuthenticationDelegate() = default;
ChromeWebAuthenticationDelegate::~ChromeWebAuthenticationDelegate() = default;

bool ChromeWebAuthenticationDelegate::
    OverrideCallerOriginAndRelyingPartyIdValidation(
        content::BrowserContext* browser_context,
        const url::Origin& caller_origin,
        const std::string& relying_party_id) {
  // Allow chrome-extensions:// origins to make WebAuthn requests.
  // `MaybeGetRelyingPartyId` will override the RP ID if it matches the
  // extension origin.
  if (caller_origin.scheme() != extensions::kExtensionScheme) {
    return false;
  }
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)
          ->enabled_extensions()
          .GetByID(caller_origin.host());
  if (!extension) {
    return false;
  }
  return ExtensionCanAssertRpId(*extension, relying_party_id);
}

std::optional<std::string>
ChromeWebAuthenticationDelegate::MaybeGetRelyingPartyIdOverride(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin) {
  // Extensions may claim their origin as an RP ID. In that case, we use the
  // whole origin to avoid collisions with the RP ID space for HTTPS origins.
  // Extensions may not claim other extensions RP IDs, even if they have host
  // permissions on them.
  if (caller_origin.scheme() == extensions::kExtensionScheme &&
      claimed_relying_party_id == caller_origin.host()) {
    return caller_origin.Serialize();
  }

  return std::nullopt;
}

bool ChromeWebAuthenticationDelegate::ShouldPermitIndividualAttestation(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin,
    const std::string& relying_party_id) {
  return IsOriginListedInEnterpriseAttestationSwitch(caller_origin) ||
         IsWebAuthnRPIDListedInSecurityKeyPermitAttestationPolicy(
             browser_context, relying_party_id);
}

bool ChromeWebAuthenticationDelegate::SupportsResidentKeys(
    content::RenderFrameHost* render_frame_host) {
  return true;
}

bool ChromeWebAuthenticationDelegate::IsFocused(
    content::WebContents* web_contents) {
  return web_contents->GetVisibility() == content::Visibility::VISIBLE;
}

void ChromeWebAuthenticationDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailableOverride(
        content::RenderFrameHost* render_frame_host,
        base::OnceCallback<void(std::optional<bool>)> callback) {
  const bool is_google =
      render_frame_host->GetLastCommittedOrigin().DomainIs("google.com");
  content::WebAuthenticationDelegate::
      IsUserVerifyingPlatformAuthenticatorAvailableOverride(
          render_frame_host,
          base::BindOnce(
              [](base::WeakPtr<ChromeWebAuthenticationDelegate>
                     web_authentication_delegate,
                 base::WeakPtr<content::BrowserContext> browser_context,
                 const bool is_google,
                 base::OnceCallback<void(std::optional<bool>)> callback,
                 std::optional<bool> testing_api_override) {
                if (!browser_context || !web_authentication_delegate) {
                  return;
                }

                // If the testing API is active, its override takes precedence.
                if (testing_api_override) {
                  std::move(callback).Run(*testing_api_override);
                  return;
                }

                // Chrome disables platform authenticators in Guest sessions.
                // They may be available (behind an additional interstitial) in
                // Incognito mode.
                Profile* profile =
                    Profile::FromBrowserContext(browser_context.get());
                if (profile->IsGuestSession()) {
                  std::move(callback).Run(false);
                  return;
                }

                // The enclave won't allow credentials for an account to be
                // stored in GPM _in_ that account. So we don't want to tell
                // accounts.google.com that there is a platform authenticator
                // if we're later going to reject a create() request for that
                // reason. Thus we have to be conservative with IsUVPAA
                // responses on google.com and ignore the possibility of using
                // the enclave authenticator.
                if (is_google) {
                  std::move(callback).Run(std::nullopt);
                  return;
                }

                web_authentication_delegate->BrowserProvidedPasskeysAvailable(
                    browser_context.get(),
                    base::BindOnce(
                        [](base::OnceCallback<void(std::optional<bool>)>
                               callback,
                           bool available) {
                          if (available) {
                            std::move(callback).Run(true);
                            return;
                          }
                          std::move(callback).Run(std::nullopt);
                        },
                        std::move(callback)));
              },
              weak_ptr_factory_.GetWeakPtr(),
              render_frame_host->GetBrowserContext()->GetWeakPtr(), is_google,
              std::move(callback)));
}

content::WebAuthenticationRequestProxy*
ChromeWebAuthenticationDelegate::MaybeGetRequestProxy(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  auto* service = extensions::WebAuthenticationProxyService::GetIfProxyAttached(
      Profile::FromBrowserContext(browser_context));
  return service && service->IsActive(caller_origin) ? service : nullptr;
}

void ChromeWebAuthenticationDelegate::PasskeyUnrecognized(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::vector<uint8_t>& passkey_credential_id,
    const std::string& relying_party_id) {
  webauthn::PasskeyChangeQuotaTracker* quota_tracker =
      webauthn::PasskeyChangeQuotaTracker::GetInstance();
  if (base::FeatureList::IsEnabled(device::kWebAuthnSignalApiHidePasskeys)) {
    if (!quota_tracker->CanMakeChange(origin)) {
      LogSignalUnknownCredential(SignalUnknownCredentialResult::kQuotaExceeded);
      FIDO_LOG(ERROR) << "Dropping removal request from " << origin
                      << ": quota exceeded.";
      return;
    }
  }
  webauthn::PasskeyModel* passkey_store =
      PasskeyModelFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  std::string credential_id(passkey_credential_id.begin(),
                            passkey_credential_id.end());
  std::optional<sync_pb::WebauthnCredentialSpecifics> credential_specifics =
      passkey_store->GetPasskeyByCredentialId(relying_party_id, credential_id);
  if (!credential_specifics) {
    LogSignalUnknownCredential(SignalUnknownCredentialResult::kPasskeyNotFound);
    return;
  }
  if (base::FeatureList::IsEnabled(device::kWebAuthnSignalApiHidePasskeys)) {
    if (credential_specifics->hidden()) {
      LogSignalUnknownCredential(
          SignalUnknownCredentialResult::kPasskeyAlreadyHidden);
      return;
    }
    quota_tracker->TrackChange(origin);
    passkey_store->HidePasskey(std::move(credential_id),
                               /*hidden_time=*/base::Time::Now());
    LogSignalUnknownCredential(SignalUnknownCredentialResult::kPasskeyHidden);
  } else {
    passkey_store->DeletePasskey(std::move(credential_id), FROM_HERE);
    LogSignalUnknownCredential(SignalUnknownCredentialResult::kPasskeyRemoved);
  }
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents);
  if (manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnPasskeyDeleted();
  }
}

void ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentials(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::string& relying_party_id,
    const std::vector<uint8_t>& user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids) {
  if (base::FeatureList::IsEnabled(device::kWebAuthnSignalApiHidePasskeys)) {
    HideAndRestorePasskeys(web_contents, origin, relying_party_id, user_id,
                           all_accepted_credentials_ids);
  } else {
    DeleteUnacceptedPasskeys(web_contents, relying_party_id, user_id,
                             all_accepted_credentials_ids);
  }
}

void ChromeWebAuthenticationDelegate::UpdateUserPasskeys(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::string& relying_party_id,
    std::vector<uint8_t>& user_id,
    const std::string& name,
    const std::string& display_name) {
  webauthn::PasskeyChangeQuotaTracker* quota_tracker =
      webauthn::PasskeyChangeQuotaTracker::GetInstance();
  if (!quota_tracker->CanMakeChange(origin)) {
    LogSignalCurrentUserDetailsUpdated(
        SignalCurrentUserDetailsResult::kQuotaExceeded);
    FIDO_LOG(ERROR) << "Dropping update request from " << origin
                    << ": quota exceeded.";
    return;
  }
  webauthn::PasskeyModel* passkey_store =
      PasskeyModelFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  bool is_passkey_updated = false;
  for (const auto& passkey :
       passkey_store->GetPasskeysForRelyingPartyId(relying_party_id)) {
    if (std::vector<uint8_t>(passkey.user_id().begin(),
                             passkey.user_id().end()) == user_id &&
        (passkey.user_name() != name ||
         passkey.user_display_name() != display_name)) {
      passkey_store->UpdatePasskey(
          passkey.credential_id(),
          {.user_name = name, .user_display_name = display_name},
          /*updated_by_user=*/false);
      is_passkey_updated = true;
    }
  }
  if (is_passkey_updated) {
    FIDO_LOG(EVENT) << "Updating passkey user details for " << origin;
    quota_tracker->TrackChange(origin);
    PasswordsClientUIDelegate* manage_passwords_ui_controller =
        PasswordsClientUIDelegateFromWebContents(web_contents);
    if (manage_passwords_ui_controller) {
      manage_passwords_ui_controller->OnPasskeyUpdated(relying_party_id);
    }
  }
  LogSignalCurrentUserDetailsUpdated(
      is_passkey_updated ? SignalCurrentUserDetailsResult::kPasskeyUpdated
                         : SignalCurrentUserDetailsResult::kPasskeyNotUpdated);
}

#if BUILDFLAG(IS_MAC)
// static
ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfig
ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfigForProfile(
    Profile* profile) {
  constexpr char kKeychainAccessGroup[] =
      MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING ".webauthn";

  std::string metadata_secret = profile->GetPrefs()->GetString(
      webauthn::pref_names::kWebAuthnTouchIdMetadataSecretPrefName);
  if (metadata_secret.empty() ||
      !base::Base64Decode(metadata_secret, &metadata_secret)) {
    metadata_secret = device::fido::mac::GenerateCredentialMetadataSecret();
    profile->GetPrefs()->SetString(
        webauthn::pref_names::kWebAuthnTouchIdMetadataSecretPrefName,
        base::Base64Encode(base::as_byte_span(metadata_secret)));
  }

  return TouchIdAuthenticatorConfig{
      .keychain_access_group = kKeychainAccessGroup,
      .metadata_secret = std::move(metadata_secret)};
}

std::optional<ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfig>
ChromeWebAuthenticationDelegate::GetTouchIdAuthenticatorConfig(
    content::BrowserContext* browser_context) {
  return TouchIdAuthenticatorConfigForProfile(
      Profile::FromBrowserContext(browser_context));
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
content::WebAuthenticationDelegate::ChromeOSGenerateRequestIdCallback
ChromeWebAuthenticationDelegate::GetGenerateRequestIdCallback(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* window =
      render_frame_host->GetNativeView()->GetToplevelWindow();
  return chromeos::webauthn::WebAuthnRequestRegistrar::Get()
      ->GetRegisterCallback(window);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void ChromeWebAuthenticationDelegate::BrowserProvidedPasskeysAvailable(
    content::BrowserContext* browser_context,
    base::OnceCallback<void(bool)> callback) {
  auto* profile =
      Profile::FromBrowserContext(browser_context)->GetOriginalProfile();
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    FIDO_LOG(EVENT)
        << "Enclave authenticator disabled because no suitable account";
    std::move(callback).Run(false);
    return;
  }

  auto* const sync_service = SyncServiceFactory::GetForProfile(profile);
  if (!sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kPasswords)) {
    FIDO_LOG(EVENT)
        << "Enclave authenticator disabled because password sync not active";
    std::move(callback).Run(false);
    return;
  }
  // Check for TPM availability.
  if (tpm_available_.has_value()) {
    std::move(callback).Run(*tpm_available_);
    return;
  }
  base::Time tpm_check_start_time = base::Time::Now();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce([]() -> bool {
        std::unique_ptr<crypto::UnexportableKeyProvider> provider =
            GetWebAuthnUnexportableKeyProvider();
        if (!provider) {
          FIDO_LOG(EVENT)
              << "Enclave authenticator disabled because no key provider";
          return false;
        }
        return provider->SelectAlgorithm(device::enclave::kSigningAlgorithms) !=
               std::nullopt;
      }),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             base::Time tpm_check_start_time,
             base::WeakPtr<ChromeWebAuthenticationDelegate> webauthn_delegate,
             bool available) {
            FIDO_LOG(DEBUG) << "Checking for TPM availability took "
                            << (base::Time::Now() - tpm_check_start_time);
            if (webauthn_delegate) {
              webauthn_delegate->tpm_available_ = available;
            }
            if (!available) {
              FIDO_LOG(EVENT)
                  << "Enclave authenticator disabled because of lack of TPM";
            }
            std::move(callback).Run(available);
          },
          std::move(callback), tpm_check_start_time,
          weak_ptr_factory_.GetWeakPtr()));
}
