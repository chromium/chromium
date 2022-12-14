// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"

#include <iterator>
#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller_webauthn_delegate.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_webauthn_credential.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"

// static
WebAuthnRequestDelegateAndroid*
WebAuthnRequestDelegateAndroid::GetRequestDelegate(
    content::WebContents* web_contents) {
  static constexpr char kWebAuthnRequestDelegateKey[] =
      "ConditionalUiDelegateKey";
  auto* delegate = static_cast<WebAuthnRequestDelegateAndroid*>(
      web_contents->GetUserData(kWebAuthnRequestDelegateKey));
  if (!delegate) {
    auto new_user_data =
        std::make_unique<WebAuthnRequestDelegateAndroid>(web_contents);
    delegate = new_user_data.get();
    web_contents->SetUserData(kWebAuthnRequestDelegateKey,
                              std::move(new_user_data));
  }

  return delegate;
}

WebAuthnRequestDelegateAndroid::WebAuthnRequestDelegateAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

WebAuthnRequestDelegateAndroid::~WebAuthnRequestDelegateAndroid() = default;

void WebAuthnRequestDelegateAndroid::OnWebAuthnRequestPending(
    content::RenderFrameHost* frame_host,
    const std::vector<device::DiscoverableCredentialMetadata>& credentials,
    bool is_conditional_request,
    base::OnceCallback<void(const std::vector<uint8_t>& id)> callback) {
  webauthn_account_selection_callback_ = std::move(callback);

  if (is_conditional_request) {
    conditional_request_in_progress_ = true;
    ReportConditionalUiPasskeyCount(credentials.size());

    ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
        content::WebContents::FromRenderFrameHost(frame_host))
        ->GetDelegateForFrame(frame_host)
        ->OnCredentialsReceived(credentials);
    return;
  }

  std::vector<TouchToFillWebAuthnCredential> display_credentials;
  base::ranges::transform(credentials, std::back_inserter(display_credentials),
                          [](const auto& credential) {
                            return TouchToFillWebAuthnCredential(
                                TouchToFillWebAuthnCredential::Username(
                                    base::UTF8ToUTF16(*credential.user.name)),
                                TouchToFillWebAuthnCredential::BackendId(
                                    base::Base64Encode(credential.cred_id)));
                          });

  if (!touch_to_fill_controller_) {
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>();
  }
  touch_to_fill_controller_->Show(
      std::vector<password_manager::UiCredential>(), display_credentials,
      std::make_unique<TouchToFillControllerWebAuthnDelegate>(this));
}

void WebAuthnRequestDelegateAndroid::CancelWebAuthnRequest(
    content::RenderFrameHost* frame_host) {
  if (conditional_request_in_progress_) {
    // Prevent autofill from offering WebAuthn credentials in the popup.
    ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
        content::WebContents::FromRenderFrameHost(frame_host))
        ->GetDelegateForFrame(frame_host)
        ->NotifyWebAuthnRequestAborted();
  } else {
    touch_to_fill_controller_->Close();
  }

  conditional_request_in_progress_ = false;
  std::move(webauthn_account_selection_callback_).Run(std::vector<uint8_t>());
}

void WebAuthnRequestDelegateAndroid::OnWebAuthnAccountSelected(
    const std::vector<uint8_t>& user_id) {
  conditional_request_in_progress_ = false;
  std::move(webauthn_account_selection_callback_).Run(user_id);
}

content::WebContents* WebAuthnRequestDelegateAndroid::web_contents() {
  return web_contents_;
}
