// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/password_credential_controller.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

namespace webauthn {

using content::AuthenticatorRequestClientDelegate;
using content::RenderFrameHost;
using password_manager::FormFetcher;
using password_manager::FormFetcherImpl;
using password_manager::PasswordForm;
using password_manager::PasswordFormDigest;
using password_manager::PasswordManagerClient;

class PasswordCredentialControllerImpl
    : public content::DocumentUserData<PasswordCredentialControllerImpl>,
      public PasswordCredentialController,
      public FormFetcher::Consumer {
 public:
  ~PasswordCredentialControllerImpl() override;

  // PasswordCredentialContainer:
  void FetchPasswords(const GURL& url,
                      PasswordCredentialsReceivedCallback callback) override;
  bool IsAuthRequired() override;
  void SetPasswordSelectedCallback(
      AuthenticatorRequestClientDelegate::PasswordSelectedCallback callback)
      override;
  void OnPasswordSelected(std::u16string username,
                          std::u16string password) override;
  base::WeakPtr<PasswordCredentialController> AsWeakPtr() override;

 private:
  explicit PasswordCredentialControllerImpl(RenderFrameHost* client);
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  // FormFetcher::Consumer:
  void OnFetchCompleted() override;

  std::unique_ptr<password_manager::FormFetcher> GetFormFetcher(
      const GURL& url);

  std::unique_ptr<password_manager::FormFetcher> form_fetcher_;
  PasswordCredentialsReceivedCallback callback_;
  content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
      password_selected_callback_;

  base::WeakPtrFactory<PasswordCredentialControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_IMPL_H_
