// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include <memory>

#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

StubPasswordManagerClient::StubPasswordManagerClient()
    : ukm_source_id_(ukm::UkmRecorder::GetNewSourceID()) {}

StubPasswordManagerClient::~StubPasswordManagerClient() {}

bool StubPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  return false;
}

bool StubPasswordManagerClient::ShowOnboarding(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save) {
  return false;
}

void StubPasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool update_password) {}

void StubPasswordManagerClient::HideManualFallbackForSaving() {}

void StubPasswordManagerClient::FocusedInputChanged(
    password_manager::PasswordManagerDriver* driver,
    autofill::mojom::FocusedFieldType focused_field_type) {}

bool StubPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin,
    const CredentialsCallback& callback) {
  return false;
}

void StubPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {}

void StubPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {}

void StubPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    const autofill::PasswordForm& form) {}

void StubPasswordManagerClient::NotifyStorePasswordCalled() {}

void StubPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> saved_manager) {}

PrefService* StubPasswordManagerClient::GetPrefs() const {
  return nullptr;
}

PasswordStore* StubPasswordManagerClient::GetProfilePasswordStore() const {
  return nullptr;
}

PasswordStore* StubPasswordManagerClient::GetAccountPasswordStore() const {
  return nullptr;
}

const GURL& StubPasswordManagerClient::GetLastCommittedEntryURL() const {
  return GURL::EmptyGURL();
}

const CredentialsFilter* StubPasswordManagerClient::GetStoreResultFilter()
    const {
  return &credentials_filter_;
}

const autofill::LogManager* StubPasswordManagerClient::GetLogManager() const {
  return &log_manager_;
}

const PasswordFeatureManager*
StubPasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

const MockPasswordFeatureManager*
StubPasswordManagerClient::GetMockPasswordFeatureManager() const {
  return &password_feature_manager_;
}

#if defined(ON_FOCUS_PING_ENABLED) || \
    defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
safe_browsing::PasswordProtectionService*
StubPasswordManagerClient::GetPasswordProtectionService() const {
  return nullptr;
}
#endif

#if defined(ON_FOCUS_PING_ENABLED)
void StubPasswordManagerClient::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {}
#endif

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
void StubPasswordManagerClient::CheckProtectedPasswordEntry(
    metrics_util::PasswordType reused_password_type,
    const std::string& username,
    const std::vector<std::string>& matching_domains,
    bool password_field_exists) {}
#endif

#if defined(SYNC_PASSWORD_REUSE_WARNING_ENABLED)
void StubPasswordManagerClient::LogPasswordReuseDetectedEvent() {}
#endif

ukm::SourceId StubPasswordManagerClient::GetUkmSourceId() {
  return ukm_source_id_;
}

PasswordManagerMetricsRecorder*
StubPasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(GetUkmSourceId(), GetMainFrameURL());
  }
  return base::OptionalOrNullptr(metrics_recorder_);
}

signin::IdentityManager* StubPasswordManagerClient::GetIdentityManager() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
StubPasswordManagerClient::GetURLLoaderFactory() {
  return nullptr;
}

bool StubPasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  return false;
}

bool StubPasswordManagerClient::IsNewTabPage() const {
  return false;
}

FieldInfoManager* StubPasswordManagerClient::GetFieldInfoManager() const {
  return nullptr;
}

}  // namespace password_manager
