// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include <memory>

#include "base/types/optional_util.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

StubPasswordManagerClient::StubPasswordManagerClient()
    : ukm_source_id_(ukm::UkmRecorder::GetNewSourceID()) {}

StubPasswordManagerClient::~StubPasswordManagerClient() = default;

bool StubPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  return false;
}

void StubPasswordManagerClient::PromptUserToMovePasswordToAccount(
    std::unique_ptr<PasswordFormManagerForUI> form_to_move) {}

void StubPasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool update_password) {}

void StubPasswordManagerClient::HideManualFallbackForSaving() {}

void StubPasswordManagerClient::FocusedInputChanged(
    password_manager::PasswordManagerDriver* driver,
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {}

bool StubPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<PasswordForm>> local_forms,
    const url::Origin& origin,
    CredentialsCallback callback) {
  return false;
}

void StubPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<PasswordForm>> local_forms,
    const url::Origin& origin) {}

void StubPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<PasswordForm> form) {}

void StubPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    std::unique_ptr<PasswordFormManagerForUI> submitted_manager) {}

void StubPasswordManagerClient::NotifyStorePasswordCalled() {}

void StubPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> saved_manager,
    bool is_update_confirmation) {}

PrefService* StubPasswordManagerClient::GetPrefs() const {
  return nullptr;
}

PrefService* StubPasswordManagerClient::GetLocalStatePrefs() const {
  return nullptr;
}

const syncer::SyncService* StubPasswordManagerClient::GetSyncService() const {
  return nullptr;
}

PasswordStoreInterface* StubPasswordManagerClient::GetProfilePasswordStore()
    const {
  return nullptr;
}

PasswordStoreInterface* StubPasswordManagerClient::GetAccountPasswordStore()
    const {
  return nullptr;
}

PasswordReuseManager* StubPasswordManagerClient::GetPasswordReuseManager()
    const {
  return nullptr;
}

MockPasswordChangeSuccessTracker*
StubPasswordManagerClient::GetPasswordChangeSuccessTracker() {
  return &password_change_success_tracker_;
}

const GURL& StubPasswordManagerClient::GetLastCommittedURL() const {
  return GURL::EmptyGURL();
}

url::Origin StubPasswordManagerClient::GetLastCommittedOrigin() const {
  return url::Origin();
}

const CredentialsFilter* StubPasswordManagerClient::GetStoreResultFilter()
    const {
  return &credentials_filter_;
}

autofill::LogManager* StubPasswordManagerClient::GetLogManager() {
  return &log_manager_;
}

const MockPasswordFeatureManager*
StubPasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

MockPasswordFeatureManager*
StubPasswordManagerClient::GetPasswordFeatureManager() {
  return &password_feature_manager_;
}

safe_browsing::PasswordProtectionService*
StubPasswordManagerClient::GetPasswordProtectionService() const {
  return nullptr;
}

#if defined(ON_FOCUS_PING_ENABLED)
void StubPasswordManagerClient::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {}
#endif

ukm::SourceId StubPasswordManagerClient::GetUkmSourceId() {
  return ukm_source_id_;
}

PasswordManagerMetricsRecorder*
StubPasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(GetUkmSourceId());
  }
  return base::OptionalToPtr(metrics_recorder_);
}

signin::IdentityManager* StubPasswordManagerClient::GetIdentityManager() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
StubPasswordManagerClient::GetURLLoaderFactory() {
  return nullptr;
}

network::mojom::NetworkContext* StubPasswordManagerClient::GetNetworkContext()
    const {
  return nullptr;
}

bool StubPasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  return false;
}

bool StubPasswordManagerClient::IsNewTabPage() const {
  return false;
}

version_info::Channel StubPasswordManagerClient::GetChannel() const {
  return version_info::Channel::UNKNOWN;
}

}  // namespace password_manager
