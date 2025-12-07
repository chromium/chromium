// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/chrome_webauthn_client_android.h"

#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

ChromeWebAuthnClientAndroid::ChromeWebAuthnClientAndroid() = default;

ChromeWebAuthnClientAndroid::~ChromeWebAuthnClientAndroid() = default;

void ChromeWebAuthnClientAndroid::OnWebAuthnRequestPending(
    content::RenderFrameHost* frame_host,
    std::vector<device::DiscoverableCredentialMetadata> credentials,
    webauthn::AssertionMediationType mediation_type,
    base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
        passkey_callback,
    base::RepeatingCallback<void(std::u16string_view, std::u16string_view)>
        password_callback,
    base::RepeatingClosure hybrid_closure,
    base::RepeatingCallback<void(webauthn::NonCredentialReturnReason)>
        non_credential_callback) {
  WebAuthnRequestDelegateAndroid* delegate =
      WebAuthnRequestDelegateAndroid::GetRequestDelegate(
          content::WebContents::FromRenderFrameHost(frame_host));

  delegate->OnWebAuthnRequestPending(
      frame_host, std::move(credentials), mediation_type,
      std::move(passkey_callback), std::move(password_callback),
      std::move(hybrid_closure), std::move(non_credential_callback));
}

void ChromeWebAuthnClientAndroid::CleanupWebAuthnRequest(
    content::RenderFrameHost* frame_host) {
  WebAuthnRequestDelegateAndroid* webauthn_request_delegate =
      WebAuthnRequestDelegateAndroid::GetRequestDelegate(
          content::WebContents::FromRenderFrameHost(frame_host));
  webauthn_request_delegate->CleanupWebAuthnRequest(frame_host);
}
