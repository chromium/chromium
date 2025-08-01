// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_webauthn_delegate.h"
#include "chrome/browser/webauthn/password_credential_controller.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller_impl.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"
#include "content/public/browser/render_frame_host.h"
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

// TODO(https://crbug.com/434882145): The logic here has gotten pretty complex
// and we should add unit tests to cover it.
void WebAuthnRequestDelegateAndroid::OnWebAuthnRequestPending(
    content::RenderFrameHost* frame_host,
    std::vector<device::DiscoverableCredentialMetadata> credentials,
    webauthn::AssertionMediationType mediation_type,
    base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
        passkey_or_dismiss_callback,
    base::RepeatingCallback<void(std::u16string_view, std::u16string_view)>
        password_callback,
    base::RepeatingCallback<void()> hybrid_callback,
    base::RepeatingCallback<void(webauthn::ImmediateRequestRejectionReason)>
        reject_immediate_callback) {
  passkey_or_dismiss_callback_ = std::move(passkey_or_dismiss_callback);
  password_callback_ = std::move(password_callback);
  hybrid_callback_ = std::move(hybrid_callback);
  reject_immediate_callback_ = std::move(reject_immediate_callback);

  std::vector<PasskeyCredential> passkey_credentials;
  std::ranges::transform(
      credentials, std::back_inserter(passkey_credentials),
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

  bool is_immediate = false;
  switch (mediation_type) {
    case webauthn::AssertionMediationType::kConditional: {
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
          std::move(passkey_credentials),
          ChromeWebAuthnCredentialsDelegate::SecurityKeyOrHybridFlowAvailable(
              !hybrid_callback_.is_null()));
      return;
    }
    case webauthn::AssertionMediationType::kImmediateWithPasswords: {
      // Only valid for the main frame.
      if (!password_controller_ && frame_host->IsInPrimaryMainFrame()) {
        password_controller_ = std::make_unique<PasswordCredentialController>(
            frame_host->GetGlobalId());
      }
      password_controller_->FetchPasswords(
          frame_host->GetLastCommittedURL(),
          base::BindOnce(
              &WebAuthnRequestDelegateAndroid::MaybeShowTouchToFillSheet,
              weak_ptr_factory_.GetWeakPtr(), frame_host->GetGlobalId(),
              /*is_immediate=*/true, std::move(passkey_credentials)));
      return;
    }
    case webauthn::AssertionMediationType::kImmediatePasskeysOnly:
      CHECK(!passkey_credentials.empty());
      is_immediate = true;
      break;
    case webauthn::AssertionMediationType::kModal:
      break;
  }

  MaybeShowTouchToFillSheet(frame_host->GetGlobalId(), is_immediate,
                            std::move(passkey_credentials), {});
}

void WebAuthnRequestDelegateAndroid::MaybeShowTouchToFillSheet(
    content::GlobalRenderFrameHostId render_frame_host_id,
    bool is_immediate,
    std::vector<PasskeyCredential> passkey_credentials,
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        password_credentials) {
  auto* frame_host = content::RenderFrameHost::FromID(render_frame_host_id);
  if (!frame_host) {
    return;
  }

  if (is_immediate && passkey_credentials.empty() &&
      password_credentials.empty()) {
    reject_immediate_callback_.Run(
        webauthn::ImmediateRequestRejectionReason::kNoCredentials);
    return;
  }

  std::vector<password_manager::UiCredential> passwords;
  std::ranges::transform(password_credentials, std::back_inserter(passwords),
                         [=](const auto& password_form) {
                           return password_manager::UiCredential(
                               *password_form,
                               frame_host->GetLastCommittedOrigin());
                         });

  if (!visibility_controller_) {
    visibility_controller_ = std::make_unique<
        password_manager::KeyboardReplacingSurfaceVisibilityControllerImpl>();
  }
  if (!touch_to_fill_controller_) {
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>(
        Profile::FromBrowserContext(frame_host->GetBrowserContext()),
        visibility_controller_->AsWeakPtr(),
        /*grouped_credential_sheet_controller=*/nullptr);
  }
  touch_to_fill_controller_->InitData(
      passwords, std::move(passkey_credentials),
      ContentPasswordManagerDriver::GetForRenderFrameHost(frame_host)
          ->AsWeakPtrImpl());
  bool should_show_hybrid_option = !hybrid_callback_.is_null() && !is_immediate;
  touch_to_fill_controller_->Show(
      std::make_unique<TouchToFillControllerWebAuthnDelegate>(
          this, should_show_hybrid_option, is_immediate),
      WebAuthnCredManDelegateFactory::GetFactory(web_contents())
          ->GetRequestDelegate(frame_host));
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
  passkey_or_dismiss_callback_.Reset();
  password_callback_.Reset();
  hybrid_callback_.Reset();
  reject_immediate_callback_.Reset();
}

void WebAuthnRequestDelegateAndroid::OnWebAuthnAccountSelected(
    const std::vector<uint8_t>& user_id) {
  if (passkey_or_dismiss_callback_) {
    passkey_or_dismiss_callback_.Run(user_id);
  }
}

void WebAuthnRequestDelegateAndroid::OnPasswordCredentialSelected(
    const PasswordCredentialPair& password_credential) {
  if (password_callback_) {
    password_callback_.Run(password_credential.first,
                           password_credential.second);
  }
}

void WebAuthnRequestDelegateAndroid::OnCredentialSelectionDeclined() {
  if (reject_immediate_callback_) {
    reject_immediate_callback_.Run(
        webauthn::ImmediateRequestRejectionReason::kUserDismissed);
  }
}

void WebAuthnRequestDelegateAndroid::OnHybridSignInSelected() {
  if (hybrid_callback_) {
    hybrid_callback_.Run();
  }
}

content::WebContents* WebAuthnRequestDelegateAndroid::web_contents() {
  return web_contents_;
}
