// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"

#include <iterator>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_webauthn_delegate.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller_impl.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"

using password_manager::ContentPasswordManagerDriver;
using password_manager::PasskeyCredential;
using webauthn::WebAuthnCredManDelegate;
using webauthn::WebAuthnCredManDelegateFactory;

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
    base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
        get_assertion_callback,
    base::RepeatingClosure hybrid_callback) {
  get_assertion_callback_ = std::move(get_assertion_callback);
  hybrid_callback_ = std::move(hybrid_callback);

  std::vector<PasskeyCredential> display_credentials;
  base::ranges::transform(
      credentials, std::back_inserter(display_credentials),
      [](const auto& credential) {
        return PasskeyCredential(
            PasskeyCredential::Source::kAndroidPhone,
            PasskeyCredential::RpId(credential.rp_id),
            PasskeyCredential::CredentialId(credential.cred_id),
            PasskeyCredential::UserId(credential.user.id),
            PasskeyCredential::Username(credential.user.name.value_or("")),
            PasskeyCredential::DisplayName(
                credential.user.display_name.value_or("")));
      });

  if (is_conditional_request) {
    conditional_request_in_progress_ = true;
    ReportConditionalUiPasskeyCount(credentials.size());
    ChromeWebAuthnCredentialsDelegate* credentials_delegate =
        ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
            content::WebContents::FromRenderFrameHost(frame_host))
            ->GetDelegateForFrame(frame_host);
    if (!credentials_delegate) {
      return;
    }
    credentials_delegate->OnCredentialsReceived(
        std::move(display_credentials),
        ChromeWebAuthnCredentialsDelegate::SecurityKeyOrHybridFlowAvailable(
            !hybrid_callback_.is_null()));
    return;
  }

  if (!visibility_controller_) {
    visibility_controller_ = std::make_unique<
        password_manager::KeyboardReplacingSurfaceVisibilityControllerImpl>();
  }
  if (!touch_to_fill_controller_) {
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>(
        Profile::FromBrowserContext(frame_host->GetBrowserContext()),
        visibility_controller_->AsWeakPtr());
  }
  touch_to_fill_controller_->Show(
      std::vector<password_manager::UiCredential>(), display_credentials,
      std::make_unique<TouchToFillControllerWebAuthnDelegate>(
          this, !hybrid_callback_.is_null()),
      WebAuthnCredManDelegateFactory::GetFactory(web_contents())
          ->GetRequestDelegate(frame_host),
      ContentPasswordManagerDriver::GetForRenderFrameHost(frame_host)
          ->AsWeakPtrImpl());
}

void WebAuthnRequestDelegateAndroid::CleanupWebAuthnRequest(
    content::RenderFrameHost* frame_host) {
  if (conditional_request_in_progress_) {
    // Prevent autofill from offering WebAuthn credentials in the popup.
    ChromeWebAuthnCredentialsDelegate* credentials_delegate =
        ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
            content::WebContents::FromRenderFrameHost(frame_host))
            ->GetDelegateForFrame(frame_host);

    if (credentials_delegate) {
      credentials_delegate->NotifyWebAuthnRequestAborted();
    }
  } else {
    touch_to_fill_controller_->Close();
  }

  conditional_request_in_progress_ = false;
  get_assertion_callback_.Reset();
}

void WebAuthnRequestDelegateAndroid::OnWebAuthnAccountSelected(
    const std::vector<uint8_t>& user_id) {
  if (get_assertion_callback_) {
    get_assertion_callback_.Run(user_id);
  }
}

void WebAuthnRequestDelegateAndroid::ShowHybridSignIn() {
  if (hybrid_callback_) {
    hybrid_callback_.Run();
  }
}

content::WebContents* WebAuthnRequestDelegateAndroid::web_contents() {
  return web_contents_;
}
