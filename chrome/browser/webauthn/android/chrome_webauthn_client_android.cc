// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/chrome_webauthn_client_android.h"

#include "chrome/browser/webauthn/android/conditional_ui_delegate_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

ChromeWebAuthnClientAndroid::ChromeWebAuthnClientAndroid() = default;

ChromeWebAuthnClientAndroid::~ChromeWebAuthnClientAndroid() = default;

void ChromeWebAuthnClientAndroid::OnWebAuthnRequestPending(
    content::RenderFrameHost* frame_host,
    const std::vector<device::DiscoverableCredentialMetadata>& credentials,
    base::OnceCallback<void(const std::vector<uint8_t>& id)> callback) {
  auto* delegate = ConditionalUiDelegateAndroid::GetConditionalUiDelegate(
      content::WebContents::FromRenderFrameHost(frame_host));

  delegate->OnWebAuthnRequestPending(credentials, std::move(callback));
}
