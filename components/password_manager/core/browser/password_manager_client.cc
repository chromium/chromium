// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

bool PasswordManagerClient::IsSavingAndFillingEnabled(const GURL& url) const {
  return true;
}

bool PasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  return true;
}

bool PasswordManagerClient::IsFillingFallbackEnabled(const GURL& url) const {
  return true;
}

void PasswordManagerClient::PostHSTSQueryForHost(const GURL& origin,
                                                 HSTSCallback callback) const {
  std::move(callback).Run(HSTSResult::kError);
}

bool PasswordManagerClient::OnCredentialManagerUsed() {
  return true;
}

void PasswordManagerClient::ShowTouchToFill(PasswordManagerDriver* driver) {}

void PasswordManagerClient::GeneratePassword() {}

void PasswordManagerClient::PasswordWasAutofilled(
    const std::vector<const autofill::PasswordForm*>& best_matches,
    const GURL& origin,
    const std::vector<const autofill::PasswordForm*>* federated_matches) {}

void PasswordManagerClient::AutofillHttpAuth(
    const autofill::PasswordForm& preferred_match,
    const PasswordFormManagerForUI* form_manager) {}

void PasswordManagerClient::NotifyUserCredentialsWereLeaked(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin) {}

SyncState PasswordManagerClient::GetPasswordSyncState() const {
  return NOT_SYNCING;
}

bool PasswordManagerClient::WasLastNavigationHTTPError() const {
  return false;
}

net::CertStatus PasswordManagerClient::GetMainFrameCertStatus() const {
  return 0;
}

void PasswordManagerClient::PromptUserToEnableAutosignin() {}

bool PasswordManagerClient::IsIncognito() const {
  return false;
}

const PasswordManager* PasswordManagerClient::GetPasswordManager() const {
  return nullptr;
}

PasswordManager* PasswordManagerClient::GetPasswordManager() {
  return const_cast<PasswordManager*>(
      static_cast<const PasswordManagerClient*>(this)->GetPasswordManager());
}

HttpAuthManager* PasswordManagerClient::GetHttpAuthManager() {
  return nullptr;
}

autofill::AutofillDownloadManager*
PasswordManagerClient::GetAutofillDownloadManager() {
  return nullptr;
}

const GURL& PasswordManagerClient::GetMainFrameURL() const {
  return GURL::EmptyGURL();
}

bool PasswordManagerClient::IsMainFrameSecure() const {
  return false;
}

const autofill::LogManager* PasswordManagerClient::GetLogManager() const {
  return nullptr;
}

void PasswordManagerClient::AnnotateNavigationEntry(bool has_password_field) {}

std::string PasswordManagerClient::GetPageLanguage() const {
  return std::string();
}

PasswordRequirementsService*
PasswordManagerClient::GetPasswordRequirementsService() {
  // Not impemented but that is a valid state as per interface definition.
  // Therefore, don't call NOTIMPLEMENTED() here.
  return nullptr;
}

favicon::FaviconService* PasswordManagerClient::GetFaviconService() {
  return nullptr;
}

bool PasswordManagerClient::IsUnderAdvancedProtection() const {
  return false;
}

}  // namespace password_manager
