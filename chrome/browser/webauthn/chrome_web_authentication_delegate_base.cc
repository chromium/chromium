// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_web_authentication_delegate_base.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "device/fido/features.h"

namespace {

bool IsCmdlineAllowedOrigin(const url::Origin& caller_origin) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin)) {
    return false;
  }
  // Note that `cmdline_allowed_origin` will be opaque if the flag is not a
  // valid URL, which won't match `caller_origin`.
  const url::Origin cmdline_allowed_origin = url::Origin::Create(
      GURL(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin)));
  return caller_origin == cmdline_allowed_origin;
}

#if !BUILDFLAG(IS_ANDROID)
bool IsGoogleCorpCrdOrigin(content::BrowserContext* browser_context,
                           const url::Origin& caller_origin) {
  // This policy explicitly does not cover external instances of CRD. It
  // must not be extended to other origins or be made configurable without going
  // through security review.
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  const PrefService* prefs = profile->GetPrefs();
  const bool google_corp_remote_proxied_request_allowed =
      prefs->GetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed);
  if (!google_corp_remote_proxied_request_allowed) {
    return false;
  }

  constexpr const char* const kGoogleCorpCrdOrigins[] = {
      "https://remotedesktop.corp.google.com",
      "https://remotedesktop-autopush.corp.google.com/",
      "https://remotedesktop-daily-6.corp.google.com/",
  };
  for (const char* corp_crd_origin : kGoogleCorpCrdOrigins) {
    if (caller_origin == url::Origin::Create(GURL(corp_crd_origin))) {
      return true;
    }
  }
  // An additional origin can be passed on the command line for testing.
  return IsCmdlineAllowedOrigin(caller_origin);
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool IsAllowedByPlatformEnterprisePolicy(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  const PrefService* prefs = profile->GetPrefs();
  const base::Value::List& allowed_origins =
      prefs->GetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins);
  if (std::ranges::any_of(
          allowed_origins, [&caller_origin](const base::Value& origin_value) {
            return caller_origin ==
                   url::Origin::Create(GURL(origin_value.GetString()));
          })) {
    return true;
  }
  if (!allowed_origins.empty()) {
    // An additional origin can be passed on the command line for testing, only
    // when the list of origins set by policy is not empty.
    return IsCmdlineAllowedOrigin(caller_origin);
  }
  return false;
}

}  // namespace

ChromeWebAuthenticationDelegateBase::ChromeWebAuthenticationDelegateBase() =
    default;
ChromeWebAuthenticationDelegateBase::~ChromeWebAuthenticationDelegateBase() =
    default;

bool ChromeWebAuthenticationDelegateBase::
    OriginMayUseRemoteDesktopClientOverride(
        content::BrowserContext* browser_context,
        const url::Origin& caller_origin) {
  // Allow an origin access to the RemoteDesktopClientOverride extension and
  // make WebAuthn requests on behalf of other origins, if a any of the
  // following are true:
  //   - The origin is explicitly allowed by a device/platform-level enterprise
  //     policy.
  //   - The origin is a Google-internal Chrome Remote Desktop origin and is
  //     allowed by a corresponding enterprise policy.
  //   - Either policy is active, and the origin matches the one provided by
  //     the command-line flag
  //    `--webauthn-remote-proxied-requests-allowed-additional-origin`, which
  //     is intended for testing purposes.

  // Check if the origin is explicitly allowed by (device/platform level)
  // enterprise policy, (or allowed by the command-line flag for testing).
  if (IsAllowedByPlatformEnterprisePolicy(browser_context, caller_origin)) {
    // TODO(crbug.com/391132173): Record UMA to track how often this policy is
    // used.
    return true;
  }

#if !BUILDFLAG(IS_ANDROID)
  // Check if the origin is a Google Corp Chrome Remote Desktop origin and
  // allowed by policy, (or allowed by the command-line flag for testing).
  if (IsGoogleCorpCrdOrigin(browser_context, caller_origin)) {
    return true;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  return false;
}

bool ChromeWebAuthenticationDelegateBase::
    OverrideCallerOriginAndRelyingPartyIdValidation(
        content::BrowserContext* browser_context,
        const url::Origin& caller_origin,
        const std::string& relying_party_id) {
  NOTIMPLEMENTED();
  return false;
}
std::optional<std::string>
ChromeWebAuthenticationDelegateBase::MaybeGetRelyingPartyIdOverride(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin) {
  NOTIMPLEMENTED();
  return std::nullopt;
}
bool ChromeWebAuthenticationDelegateBase::ShouldPermitIndividualAttestation(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin,
    const std::string& relying_party_id) {
  NOTIMPLEMENTED();
  return false;
}
bool ChromeWebAuthenticationDelegateBase::SupportsResidentKeys(
    content::RenderFrameHost* render_frame_host) {
  NOTIMPLEMENTED();
  return false;
}
bool ChromeWebAuthenticationDelegateBase::IsFocused(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
  return false;
}
void ChromeWebAuthenticationDelegateBase::
    IsUserVerifyingPlatformAuthenticatorAvailableOverride(
        content::RenderFrameHost* render_frame_host,
        base::OnceCallback<void(std::optional<bool>)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(std::nullopt);
}
content::WebAuthenticationRequestProxy*
ChromeWebAuthenticationDelegateBase::MaybeGetRequestProxy(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  NOTIMPLEMENTED();
  return nullptr;
}
void ChromeWebAuthenticationDelegateBase::PasskeyUnrecognized(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::vector<uint8_t>& passkey_credential_id,
    const std::string& relying_party_id) {
  NOTIMPLEMENTED();
}
void ChromeWebAuthenticationDelegateBase::SignalAllAcceptedCredentials(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::string& relying_party_id,
    const std::vector<uint8_t>& user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids) {
  NOTIMPLEMENTED();
}
void ChromeWebAuthenticationDelegateBase::UpdateUserPasskeys(
    content::WebContents* web_contents,
    const url::Origin& origin,
    const std::string& relying_party_id,
    std::vector<uint8_t>& user_id,
    const std::string& name,
    const std::string& display_name) {
  NOTIMPLEMENTED();
}
void ChromeWebAuthenticationDelegateBase::BrowserProvidedPasskeysAvailable(
    content::BrowserContext* browser_context,
    base::OnceCallback<void(bool)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}
