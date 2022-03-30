// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_conditional_ui_delegate_android.h"

#include <memory>

#include "base/callback.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"

// static
ChromeConditionalUiDelegateAndroid*
ChromeConditionalUiDelegateAndroid::GetConditionalUiDelegate(
    content::WebContents* web_contents) {
  static constexpr char kConditionalUiDelegateKey[] =
      "ConditionalUiDelegateKey";
  auto* delegate = static_cast<ChromeConditionalUiDelegateAndroid*>(
      web_contents->GetUserData(kConditionalUiDelegateKey));
  if (!delegate) {
    auto new_user_data = std::make_unique<ChromeConditionalUiDelegateAndroid>();
    delegate = new_user_data.get();
    web_contents->SetUserData(kConditionalUiDelegateKey,
                              std::move(new_user_data));
  }

  return delegate;
}

ChromeConditionalUiDelegateAndroid::ChromeConditionalUiDelegateAndroid() {}

ChromeConditionalUiDelegateAndroid::~ChromeConditionalUiDelegateAndroid() {}

void ChromeConditionalUiDelegateAndroid::OnWebAuthnRequestPending(
    const std::vector<device::DiscoverableCredentialMetadata>& credentials,
    base::OnceCallback<void(const std::vector<uint8_t>& id)> callback) {
  webauthn_account_selection_callback_ = std::move(callback);
  webauthn_account_suggestions_ = std::move(credentials);

  if (retrieve_credentials_callback_) {
    std::move(retrieve_credentials_callback_)
        .Run(webauthn_account_suggestions_);
  }
}

void ChromeConditionalUiDelegateAndroid::OnWebAuthnAccountSelected(
    const std::vector<uint8_t>& user_id) {
  std::move(webauthn_account_selection_callback_).Run(user_id);
}

void ChromeConditionalUiDelegateAndroid::RetrieveWebAuthnCredentials(
    base::OnceCallback<void(
        const std::vector<device::DiscoverableCredentialMetadata>&)> callback) {
  // Complete immediately if there is an outstanding WebAuthn get request.
  if (webauthn_account_selection_callback_) {
    std::move(callback).Run(webauthn_account_suggestions_);
    return;
  }

  retrieve_credentials_callback_ = std::move(callback);
}
