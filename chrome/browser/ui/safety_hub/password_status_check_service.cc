// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_service.h"

#include <memory>

#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

PasswordStatusCheckService::PasswordStatusCheckService(Profile* profile)
    : profile_(profile) {}

PasswordStatusCheckService::~PasswordStatusCheckService() = default;

void PasswordStatusCheckService::Shutdown() {
  saved_passwords_presenter_observation_.Reset();
  saved_passwords_presenter_.reset();
}

void PasswordStatusCheckService::UpdateInsecureCredentialCountAsync() {
  saved_passwords_presenter_observation_.Reset();

  saved_passwords_presenter_ =
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          AffiliationServiceFactory::GetForProfile(profile_),
          PasswordStoreFactory::GetForProfile(
              profile_, ServiceAccessType::IMPLICIT_ACCESS),
          AccountPasswordStoreFactory::GetForProfile(
              profile_, ServiceAccessType::IMPLICIT_ACCESS));
  saved_passwords_presenter_observation_.Observe(
      saved_passwords_presenter_.get());
  saved_passwords_presenter_->Init();
}

void PasswordStatusCheckService::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  extensions::IdGenerator credential_id_generator;
  auto password_check_delegate =
      std::make_unique<extensions::PasswordCheckDelegate>(
          profile_, saved_passwords_presenter_.get(), &credential_id_generator);

  std::vector<password_manager::CredentialUIEntry> insecure_credentials =
      password_check_delegate->GetInsecureCredentialsManager()
          ->GetInsecureCredentialEntries();

  compromised_credential_count_ = 0;
  weak_credential_count_ = 0;
  reused_credential_count_ = 0;

  for (const auto& entry : insecure_credentials) {
    if (entry.IsMuted()) {
      continue;
    }
    if (password_manager::IsCompromised(entry)) {
      compromised_credential_count_++;
    } else if (entry.IsWeak()) {
      weak_credential_count_++;
    } else if (entry.IsReused()) {
      reused_credential_count_++;
    }
  }

  password_check_delegate.reset();
  saved_passwords_presenter_observation_.Reset();
  saved_passwords_presenter_.reset();

  if (on_passwords_changed_finished_callback_for_test_) {
    on_passwords_changed_finished_callback_for_test_.Run();
  }
}

void PasswordStatusCheckService::RunPasswordCheck() {
  // TODO(crbug.com/1443466)
  NOTIMPLEMENTED();
}
