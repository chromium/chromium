// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/password_credential_controller.h"

#include "base/check.h"
#include "base/check_is_test.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/webauthn/password_credential_controller_impl.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/render_frame_host.h"

namespace webauthn {

// static
PasswordCredentialController* PasswordCredentialController::MaybeGet(
    RenderFrameHost* render_frame_host) {
  if (PasswordCredentialController::g_instance_for_testing_ != nullptr) {
    CHECK_IS_TEST();
    return g_instance_for_testing_;
  }

  // Only valid for the main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return nullptr;
  }

  return PasswordCredentialControllerImpl::GetOrCreateForCurrentDocument(
      render_frame_host);
}

void PasswordCredentialController::FetchPasswords(
    const GURL& url,
    PasswordCredentialsReceivedCallback callback) {}

bool PasswordCredentialController::IsAuthRequired() {
  return false;
}

void PasswordCredentialController::SetPasswordSelectedCallback(
    AuthenticatorRequestClientDelegate::PasswordSelectedCallback callback) {}

void PasswordCredentialController::OnPasswordSelected(std::u16string username,
                                                      std::u16string password) {
}

// static
void PasswordCredentialController::set_instance_for_testing(
    PasswordCredentialController* instance) {
  g_instance_for_testing_ = instance;
}

PasswordCredentialController*
    PasswordCredentialController::g_instance_for_testing_ = nullptr;

}  // namespace webauthn
