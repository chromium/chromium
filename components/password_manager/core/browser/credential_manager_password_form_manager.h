// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace password_manager {

class PasswordManagerClient;
struct PasswordForm;

// A delegate that is notified when CredentialManagerPasswordFormManager
// finishes working with password forms.
class CredentialManagerPasswordFormManagerDelegate {
 public:
  // Called when a password form has been provisionally saved.
  virtual void OnProvisionalSaveComplete() = 0;
};

// A PasswordFormManager built to handle PasswordForm objects synthesized
// by the Credential Management API.
class CredentialManagerPasswordFormManager : public PasswordFormManager {
 public:
  // Given a |client| and an |observed_form|, kick off the process of fetching
  // matching logins from the password store; if |observed_form| doesn't map to
  // a blocklisted origin, provisionally save |saved_form|. Once saved, let the
  // delegate know that it's safe to poke at the UI. |form_fetcher| is passed
  // to PasswordFormManager. |form_saver| can be null, in which case it is
  // created automatically.
  //
  // This class does not take ownership of |delegate|.
  CredentialManagerPasswordFormManager(
      PasswordManagerClient* client,
      std::unique_ptr<PasswordForm> saved_form,
      CredentialManagerPasswordFormManagerDelegate* delegate,
      std::unique_ptr<FormSaver> form_saver,
      std::unique_ptr<FormFetcher> form_fetcher);
  CredentialManagerPasswordFormManager(
      const CredentialManagerPasswordFormManager&) = delete;
  CredentialManagerPasswordFormManager& operator=(
      const CredentialManagerPasswordFormManager&) = delete;
  ~CredentialManagerPasswordFormManager() override;

  // FormFetcher::Consumer:
  void OnFetchCompleted() override;

  // PasswordFormManagerForUI:
  metrics_util::CredentialSourceType GetCredentialSource() const override;

 private:
  // Calls OnProvisionalSaveComplete on |delegate_|.
  void NotifyDelegate();

  const raw_ptr<CredentialManagerPasswordFormManagerDelegate> delegate_;

  base::WeakPtrFactory<CredentialManagerPasswordFormManager> weak_factory_{
      this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_
