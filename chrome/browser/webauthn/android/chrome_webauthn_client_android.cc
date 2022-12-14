// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/chrome_webauthn_client_android.h"

#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

ChromeWebAuthnClientAndroid::ChromeWebAuthnClientAndroid() = default;

ChromeWebAuthnClientAndroid::~ChromeWebAuthnClientAndroid() = default;

void ChromeWebAuthnClientAndroid::OnWebAuthnRequestPending(
    content::RenderFrameHost* frame_host,
    const std::vector<device::DiscoverableCredentialMetadata>& credentials,
    bool is_conditional_request,
    base::OnceCallback<void(const std::vector<uint8_t>& id)> callback) {
  auto* delegate = WebAuthnRequestDelegateAndroid::GetRequestDelegate(
      content::WebContents::FromRenderFrameHost(frame_host));

  delegate->OnWebAuthnRequestPending(
      frame_host, credentials, is_conditional_request, std::move(callback));
}

void ChromeWebAuthnClientAndroid::CancelWebAuthnRequest(
    content::RenderFrameHost* frame_host) {
  auto* delegate = WebAuthnRequestDelegateAndroid::GetRequestDelegate(
      content::WebContents::FromRenderFrameHost(frame_host));
  delegate->CancelWebAuthnRequest(frame_host);
}
