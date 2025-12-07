// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_WEB_AUTHENTICATION_DELEGATE_BASE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_WEB_AUTHENTICATION_DELEGATE_BASE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "content/public/browser/web_authentication_request_proxy.h"

// ChromeWebAuthenticationDelegateBase is the base class for //chrome layer's
// implementation of content::WebAuthenticationDelegate. It provides common
// functionality that is shared between the Android and Desktop platforms.
class ChromeWebAuthenticationDelegateBase
    : public content::WebAuthenticationDelegate {
 public:
  ChromeWebAuthenticationDelegateBase();

  ~ChromeWebAuthenticationDelegateBase() override;

  // This methods are a final overrides of content::WebAuthenticationDelegate,
  // providing an implementation that is shared between both Desktop and
  // Android platforms.
  bool OriginMayUseRemoteDesktopClientOverride(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) final;

  // The following methods are part of content::WebAuthenticationDelegate, but
  // their implementation is platform-specific. Therefore, they are not
  // implemented in this base class and must be overridden in the subclasses
  // that target specific platforms:
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
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_WEB_AUTHENTICATION_DELEGATE_BASE_H_
