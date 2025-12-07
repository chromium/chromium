// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_UI_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_UI_CONTROLLER_H_

#include <memory>
#include <string>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/shared_types.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class RenderFrameHost;
}

namespace password_manager {
class PasswordManagerClient;
}

// Manages the UI aspects of using password credentials within the WebAuthn
// dialog. This includes populating the list of password credentials in the UI
// model and handling OS re-authentication. This class is not used on Android.
class PasswordCredentialUIController
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit PasswordCredentialUIController(
      content::GlobalRenderFrameHostId render_frame_host_id,
      AuthenticatorRequestDialogModel* model);
  ~PasswordCredentialUIController() override;

  PasswordCredentialUIController(const PasswordCredentialUIController&) =
      delete;
  PasswordCredentialUIController& operator=(
      const PasswordCredentialUIController&) = delete;

  // Returns `true` if the user is required to pass screen lock before using a
  // credential.
  virtual bool IsAuthRequired();

  virtual void SetPasswordSelectedCallback(
      content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
          callback);

  // AuthenticatorRequestDialogModel::Observer
  void OnPasswordCredentialSelected(PasswordCredentialPair password) override;
  void OnStepTransition() override;
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

  void SetPasswordManagerClientForTesting(
      password_manager::PasswordManagerClient* client);

 private:
  password_manager::PasswordManagerClient* GetPasswordManagerClient() const;
  content::RenderFrameHost* GetRenderFrameHost() const;
  void OnAuthenticationCompleted(PasswordCredentialPair password, bool success);

  const content::GlobalRenderFrameHostId render_frame_host_id_;

  raw_ptr<password_manager::PasswordManagerClient> client_for_testing_;
  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observer_{this};
  content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
      password_selected_callback_;
  std::optional<PasswordCredentialPair> filling_password_;

  base::WeakPtrFactory<PasswordCredentialUIController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_UI_CONTROLLER_H_
