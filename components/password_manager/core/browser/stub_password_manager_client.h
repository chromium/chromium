// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

// Use this class as a base for mock or test clients to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs.  Do NOT use in production, only use in tests.
class StubPasswordManagerClient : public PasswordManagerClient {
 public:
  StubPasswordManagerClient();
  ~StubPasswordManagerClient() override;

  // PasswordManagerClient:
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool update_password) override;
  bool ShowOnboarding(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save) override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool update_password) override;
  void HideManualFallbackForSaving() override;
  void FocusedInputChanged(
      password_manager::PasswordManagerDriver* driver,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm>) override;
  void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form) override;
  void NotifyStorePasswordCalled() override;
  void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManagerForUI> saved_manager) override;
  PrefService* GetPrefs() const override;
  PasswordStore* GetProfilePasswordStore() const override;
  PasswordStore* GetAccountPasswordStore() const override;
  const GURL& GetLastCommittedEntryURL() const override;
  const CredentialsFilter* GetStoreResultFilter() const override;
  const autofill::LogManager* GetLogManager() const override;
  const PasswordFeatureManager* GetPasswordFeatureManager() const override;
  const MockPasswordFeatureManager* GetMockPasswordFeatureManager() const;

#if defined(ON_FOCUS_PING_ENABLED) || \
    defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;
#endif

#if defined(ON_FOCUS_PING_ENABLED)
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
#endif

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  void CheckProtectedPasswordEntry(
      metrics_util::PasswordType reused_password_type,
      const std::string& username,
      const std::vector<std::string>& matching_domains,
      bool password_field_exists) override;
#endif

#if defined(SYNC_PASSWORD_REUSE_WARNING_ENABLED)
  void LogPasswordReuseDetectedEvent() override;
#endif

  ukm::SourceId GetUkmSourceId() override;
  PasswordManagerMetricsRecorder* GetMetricsRecorder() override;
  signin::IdentityManager* GetIdentityManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool IsIsolationForPasswordSitesEnabled() const override;
  bool IsNewTabPage() const override;
  FieldInfoManager* GetFieldInfoManager() const override;

 private:
  const StubCredentialsFilter credentials_filter_;
  testing::NiceMock<MockPasswordFeatureManager> password_feature_manager_;
  autofill::StubLogManager log_manager_;
  ukm::SourceId ukm_source_id_;
  base::Optional<PasswordManagerMetricsRecorder> metrics_recorder_;

  DISALLOW_COPY_AND_ASSIGN(StubPasswordManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
