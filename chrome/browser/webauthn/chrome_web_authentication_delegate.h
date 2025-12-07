// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_WEB_AUTHENTICATION_DELEGATE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_WEB_AUTHENTICATION_DELEGATE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/chrome_web_authentication_delegate_base.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "url/origin.h"

// ChromeWebAuthenticationDelegate is the //chrome layer implementation
// of content::WebAuthenticationDelegate.
class ChromeWebAuthenticationDelegate final
    : public ChromeWebAuthenticationDelegateBase {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SignalUnknownCredentialResult)
  enum class SignalUnknownCredentialResult {
    kPasskeyNotFound = 0,
    kPasskeyRemoved = 1,
    kPasskeyHidden = 2,
    kQuotaExceeded = 3,
    kPasskeyAlreadyHidden = 4,
    kMaxValue = kPasskeyAlreadyHidden,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:SignalUnknownCredentialResultEnum)

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SignalAllAcceptedCredentialsResult)
  enum class SignalAllAcceptedCredentialsResult {
    kNoPasskeyChanged = 0,
    kPasskeyRemoved = 1,
    kPasskeyHidden = 2,
    kPasskeyRestored = 3,
    kQuotaExceeded = 4,
    kMaxValue = kQuotaExceeded,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:SignalAllAcceptedCredentialsResultEnum)

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SignalCurrentUserDetailsResult)
  enum class SignalCurrentUserDetailsResult {
    kQuotaExceeded = 0,
    kPasskeyUpdated = 1,
    kPasskeyNotUpdated = 2,
    kMaxValue = kPasskeyNotUpdated,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:SignalCurrentUserDetailsResultEnum)

#if BUILDFLAG(IS_MAC)
  // Returns a configuration struct for instantiating the macOS WebAuthn
  // platform authenticator for the given Profile.
  static TouchIdAuthenticatorConfig TouchIdAuthenticatorConfigForProfile(
      Profile* profile);
#endif  // BUILDFLAG(IS_MAC)

  ChromeWebAuthenticationDelegate();

  ~ChromeWebAuthenticationDelegate() override;

  // content::WebAuthenticationDelegate:
  bool OverrideCallerOriginAndRelyingPartyIdValidation(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id) override;
  std::optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_relying_party_id,
      const url::Origin& caller_origin) override;
  bool ShouldPermitIndividualAttestation(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id) override;
  bool SupportsResidentKeys(
      content::RenderFrameHost* render_frame_host) override;
  bool IsFocused(content::WebContents* web_contents) override;
  void IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      content::RenderFrameHost* render_frame_host,
      base::OnceCallback<void(std::optional<bool>)> callback) override;
  content::WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override;
  void PasskeyUnrecognized(content::WebContents* web_contents,
                           const url::Origin& origin,
                           const std::vector<uint8_t>& passkey_credential_id,
                           const std::string& relying_party_id) override;
  void SignalAllAcceptedCredentials(content::WebContents* web_contents,
                                    const url::Origin& origin,
                                    const std::string& relying_party_id,
                                    const std::vector<uint8_t>& user_id,
                                    const std::vector<std::vector<uint8_t>>&
                                        all_accepted_credentials_ids) override;
  void UpdateUserPasskeys(content::WebContents* web_contents,
                          const url::Origin& origin,
                          const std::string& relying_party_id,
                          std::vector<uint8_t>& user_id,
                          const std::string& name,
                          const std::string& display_name) override;
  void BrowserProvidedPasskeysAvailable(
      content::BrowserContext* browser_context,
      base::OnceCallback<void(bool)> callback) override;

#if BUILDFLAG(IS_MAC)
  std::optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig(
      content::BrowserContext* browser_context) override;
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS)
  ChromeOSGenerateRequestIdCallback GetGenerateRequestIdCallback(
      content::RenderFrameHost* render_frame_host) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  // Caches the result from looking up whether a TPM is available for Enclave
  // requests.
  std::optional<bool> tpm_available_;
  base::WeakPtrFactory<ChromeWebAuthenticationDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_WEB_AUTHENTICATION_DELEGATE_H_
