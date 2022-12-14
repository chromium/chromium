// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_CHROME_WEBAUTHN_CLIENT_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_CHROME_WEBAUTHN_CLIENT_ANDROID_H_

#include "base/callback_forward.h"
#include "components/webauthn/android/webauthn_client_android.h"

// Chrome implementation of WebAuthnClientAndroid.
class ChromeWebAuthnClientAndroid : public components::WebAuthnClientAndroid {
 public:
  ChromeWebAuthnClientAndroid();
  ~ChromeWebAuthnClientAndroid() override;

  ChromeWebAuthnClientAndroid(const ChromeWebAuthnClientAndroid&) = delete;
  ChromeWebAuthnClientAndroid& operator=(const ChromeWebAuthnClientAndroid&) =
      delete;

  // components::WebAuthnClientAndroid:
  void OnWebAuthnRequestPending(
      content::RenderFrameHost* frame_host,
      const std::vector<device::DiscoverableCredentialMetadata>& credentials,
      bool is_conditional_request,
      base::OnceCallback<void(const std::vector<uint8_t>& id)> callback)
      override;

  void CancelWebAuthnRequest(content::RenderFrameHost* frame_host) override;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_CHROME_WEBAUTHN_CLIENT_ANDROID_H_
