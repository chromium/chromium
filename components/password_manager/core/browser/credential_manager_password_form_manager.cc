// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"

namespace password_manager {

CredentialManagerPasswordFormManager::CredentialManagerPasswordFormManager(
    PasswordManagerClient* client,
    std::unique_ptr<PasswordForm> saved_form,
    CredentialManagerPasswordFormManagerDelegate* delegate,
    std::unique_ptr<FormSaver> form_saver,
    std::unique_ptr<FormFetcher> form_fetcher)
    : PasswordFormManager(
          client,
          std::move(saved_form),
          std::move(form_fetcher),
          form_saver ? std::make_unique<PasswordSaveManagerImpl>(
                           /*profile_form_saver=*/std::move(form_saver),
                           /*account_form_saver=*/nullptr)
                     : std::make_unique<PasswordSaveManagerImpl>(client)),
      delegate_(delegate) {}

CredentialManagerPasswordFormManager::~CredentialManagerPasswordFormManager() =
    default;

void CredentialManagerPasswordFormManager::OnFetchCompleted() {
  PasswordFormManager::OnFetchCompleted();

  CreatePendingCredentials();
  NotifyDelegate();
}

metrics_util::CredentialSourceType
CredentialManagerPasswordFormManager::GetCredentialSource() const {
  return metrics_util::CredentialSourceType::kCredentialManagementAPI;
}

void CredentialManagerPasswordFormManager::NotifyDelegate() {
  delegate_->OnProvisionalSaveComplete();
}

}  // namespace password_manager
