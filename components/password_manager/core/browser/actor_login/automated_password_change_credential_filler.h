// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_AUTOMATED_PASSWORD_CHANGE_CREDENTIAL_FILLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_AUTOMATED_PASSWORD_CHANGE_CREDENTIAL_FILLER_H_

#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace actor_login {

// This is a specific implementation of `ActorLoginCredentialFiller` that is
// used for automated password change flows only.
class AutomatedPasswordChangeCredentialFiller
    : public ActorLoginCredentialFiller {
 public:
  AutomatedPasswordChangeCredentialFiller(
      const url::Origin& main_frame_origin,
      const Credential& credential,
      password_manager::PasswordManagerClient* client,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_start_time,
      IsTaskInFocus is_task_in_focus,
      LoginStatusResultOrErrorReply callback,
      password_manager::StoredCredential stored_credential);

  ~AutomatedPasswordChangeCredentialFiller() override;

 protected:
  const password_manager::StoredCredential* GetMatchingStoredCredential(
      const password_manager::PasswordFormManager& signin_form_manager)
      override;

  bool DoesStoredCredentialBelongToManager(
      const password_manager::PasswordFormManager* manager,
      const password_manager::StoredCredential& stored_credential) override;

  bool IsReauthBeforeFillingRequired() override;

 private:
  password_manager::StoredCredential stored_credential_;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_AUTOMATED_PASSWORD_CHANGE_CREDENTIAL_FILLER_H_
