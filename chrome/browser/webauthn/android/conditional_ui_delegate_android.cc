// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/conditional_ui_delegate_android.h"

#include <memory>

#include "base/callback.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "third_party/blink/public/common/tokens/tokens.h"

// static
ConditionalUiDelegateAndroid*
ConditionalUiDelegateAndroid::GetConditionalUiDelegate(
    content::WebContents* web_contents) {
  static constexpr char kConditionalUiDelegateKey[] =
      "ConditionalUiDelegateKey";
  auto* delegate = static_cast<ConditionalUiDelegateAndroid*>(
      web_contents->GetUserData(kConditionalUiDelegateKey));
  if (!delegate) {
    auto new_user_data = std::make_unique<ConditionalUiDelegateAndroid>();
    delegate = new_user_data.get();
    web_contents->SetUserData(kConditionalUiDelegateKey,
                              std::move(new_user_data));
  }

  return delegate;
}

ConditionalUiDelegateAndroid::ConditionalUiDelegateAndroid() {}

ConditionalUiDelegateAndroid::~ConditionalUiDelegateAndroid() {}

void ConditionalUiDelegateAndroid::OnWebAuthnRequestPending(
    content::RenderFrameHost* frame_host,
    const std::vector<device::DiscoverableCredentialMetadata>& credentials,
    base::OnceCallback<void(const std::vector<uint8_t>& id)> callback) {
  webauthn_account_selection_callback_ = std::move(callback);

  ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
      content::WebContents::FromRenderFrameHost(frame_host))
      ->GetDelegateForFrame(frame_host)
      ->OnCredentialsReceived(credentials);
}

void ConditionalUiDelegateAndroid::CancelWebAuthnRequest(
    content::RenderFrameHost* frame_host) {
  // Calling OnCredentialsReceived() with an empty list will prevent autofill
  // from offering WebAuthn credentials in the popup.
  ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
      content::WebContents::FromRenderFrameHost(frame_host))
      ->GetDelegateForFrame(frame_host)
      ->OnCredentialsReceived(
          std::vector<device::DiscoverableCredentialMetadata>());
  std::move(webauthn_account_selection_callback_).Run(std::vector<uint8_t>());
}

void ConditionalUiDelegateAndroid::OnWebAuthnAccountSelected(
    const std::vector<uint8_t>& user_id) {
  std::move(webauthn_account_selection_callback_).Run(user_id);
}
