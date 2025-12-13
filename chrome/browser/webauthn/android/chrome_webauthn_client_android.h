// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_CHROME_WEBAUTHN_CLIENT_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_CHROME_WEBAUTHN_CLIENT_ANDROID_H_

#include "base/functional/callback_forward.h"
#include "components/webauthn/android/webauthn_client_android.h"
#include "device/fido/discoverable_credential_metadata.h"

// Chrome implementation of WebAuthnClientAndroid.
class ChromeWebAuthnClientAndroid : public webauthn::WebAuthnClientAndroid {
 public:
  ChromeWebAuthnClientAndroid();
  ~ChromeWebAuthnClientAndroid() override;

  ChromeWebAuthnClientAndroid(const ChromeWebAuthnClientAndroid&) = delete;
  ChromeWebAuthnClientAndroid& operator=(const ChromeWebAuthnClientAndroid&) =
      delete;

  // webauthn::WebAuthnClientAndroid:
  void OnWebAuthnRequestPending(
      content::RenderFrameHost* frame_host,
      std::vector<device::DiscoverableCredentialMetadata> credentials,
      webauthn::AssertionMediationType mediation_type,
      base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
          passkey_callback,
      base::RepeatingCallback<void(std::u16string_view, std::u16string_view)>
          password_callback,
      base::RepeatingClosure hybrid_closure,
      base::RepeatingCallback<void(webauthn::NonCredentialReturnReason)>
          non_credential_callback) override;

  void CleanupWebAuthnRequest(content::RenderFrameHost* frame_host) override;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_CHROME_WEBAUTHN_CLIENT_ANDROID_H_
