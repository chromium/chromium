// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/saved_info_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/web_ui.h"

namespace settings {

SavedInfoHandler::SavedInfoHandler(Profile* profile) : profile_(profile) {}

SavedInfoHandler::SavedInfoHandler(
    Profile* profile,
    std::unique_ptr<password_manager::SavedPasswordsPresenter> presenter)
    : profile_(profile), saved_passwords_presenter_(std::move(presenter)) {}

SavedInfoHandler::~SavedInfoHandler() = default;

void SavedInfoHandler::RegisterMessages() {
  if (profile_->IsOffTheRecord()) {
    return;
  }

  web_ui()->RegisterMessageCallback(
      "getPasswordCount",
      base::BindRepeating(&SavedInfoHandler::HandleGetPasswordCount,
                          base::Unretained(this)));
}

void SavedInfoHandler::OnJavascriptAllowed() {
  if (!saved_passwords_presenter_) {
    auto* affiliation_service =
        AffiliationServiceFactory::GetForProfile(profile_);
    if (!affiliation_service) {
      return;
    }
    saved_passwords_presenter_ =
        std::make_unique<password_manager::SavedPasswordsPresenter>(
            affiliation_service,
            ProfilePasswordStoreFactory::GetForProfile(
                profile_, ServiceAccessType::EXPLICIT_ACCESS),
            AccountPasswordStoreFactory::GetForProfile(
                profile_, ServiceAccessType::EXPLICIT_ACCESS));
    saved_passwords_presenter_->Init();
  }

  observation_.Reset();
  observation_.Observe(saved_passwords_presenter_.get());
}

void SavedInfoHandler::OnJavascriptDisallowed() {
  observation_.Reset();
  if (saved_passwords_presenter_) {
    saved_passwords_presenter_.reset();
  }
}

void SavedInfoHandler::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  FireWebUIListener("password-count-changed", GetPasswordCounts());
}

base::Value::Dict SavedInfoHandler::GetPasswordCounts() {
  base::Value::Dict dict;
  if (saved_passwords_presenter_) {
    size_t password_count = 0;
    size_t passkey_count = 0;
    for (const auto& credential :
         saved_passwords_presenter_->GetSavedCredentials()) {
      if (!credential.passkey_credential_id.empty()) {
        passkey_count++;
      } else {
        password_count++;
      }
    }
    dict.Set("passwordCount", static_cast<int>(password_count));
    dict.Set("passkeyCount", static_cast<int>(passkey_count));
  }
  return dict;
}

void SavedInfoHandler::HandleGetPasswordCount(const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetPasswordCounts());
}

}  // namespace settings
