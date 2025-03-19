// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/global_routing_id.h"

// This class is responsible for fetching `PasswordCredentials` for the given
// `render_frame_host` and responding with the found credentials for a `url`.
class PasswordCredentialController
    : public password_manager::FormFetcher::Consumer,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  using PasswordCredentials =
      std::vector<std::unique_ptr<password_manager::PasswordForm>>;
  using PasswordCredentialsReceivedCallback =
      base::OnceCallback<void(PasswordCredentials)>;

  PasswordCredentialController(
      content::GlobalRenderFrameHostId render_frame_host_id,
      AuthenticatorRequestDialogModel* model);
  ~PasswordCredentialController() override;

  virtual void FetchPasswords(const GURL& url,
                              PasswordCredentialsReceivedCallback callback);

  // Returns `true` if the user is required to pass screen lock before using a
  // credential.
  virtual bool IsAuthRequired();

  virtual void SetPasswordSelectedCallback(
      content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
          callback);

  // AuthenticatorRequestDialogModel::Observer
  void OnPasswordCredentialSelected(PasswordCredentialPair password) override;
  void OnStepTransition() override;

 private:
  // FormFetcher::Consumer:
  void OnFetchCompleted() override;

  std::unique_ptr<password_manager::FormFetcher> GetFormFetcher(
      const GURL& url);

  content::RenderFrameHost* GetRenderFrameHost() const;

  void OnAuthenticationCompleted(PasswordCredentialPair password, bool success);

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  raw_ptr<AuthenticatorRequestDialogModel> model_;

  std::unique_ptr<password_manager::FormFetcher> form_fetcher_;
  PasswordCredentialsReceivedCallback callback_;
  content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
      password_selected_callback_;
  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observer_{this};

  std::optional<PasswordCredentialPair> filling_password_;

  base::WeakPtrFactory<PasswordCredentialController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_
