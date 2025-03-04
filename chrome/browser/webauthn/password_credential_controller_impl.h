// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/password_credential_controller.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/document_user_data.h"

namespace webauthn {

using content::AuthenticatorRequestClientDelegate;
using content::GlobalRenderFrameHostId;
using password_manager::FormFetcher;
using password_manager::FormFetcherImpl;
using password_manager::PasswordForm;
using password_manager::PasswordFormDigest;
using password_manager::PasswordManagerClient;

class PasswordCredentialControllerImpl
    : public PasswordCredentialController,
      public FormFetcher::Consumer,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit PasswordCredentialControllerImpl(
      GlobalRenderFrameHostId render_frame_host_id,
      AuthenticatorRequestDialogModel* model);
  ~PasswordCredentialControllerImpl() override;

  // PasswordCredentialContainer:
  void FetchPasswords(const GURL& url,
                      PasswordCredentialsReceivedCallback callback) override;
  bool IsAuthRequired() override;
  void SetPasswordSelectedCallback(
      AuthenticatorRequestClientDelegate::PasswordSelectedCallback callback)
      override;

  // AuthenticatorRequestDialogModel::Observer
  void OnPasswordCredentialSelected(PasswordPair password) override;

 private:
  // FormFetcher::Consumer:
  void OnFetchCompleted() override;

  std::unique_ptr<password_manager::FormFetcher> GetFormFetcher(
      const GURL& url);

  content::RenderFrameHost* GetRenderFrameHost() const;

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  scoped_refptr<AuthenticatorRequestDialogModel> model_;

  std::unique_ptr<password_manager::FormFetcher> form_fetcher_;
  PasswordCredentialsReceivedCallback callback_;
  content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
      password_selected_callback_;
  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observer_{this};
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_IMPL_H_
