// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_

#include <optional>

#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

// Use this class as a base for mock or test clients to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs.  Do NOT use in production, only use in tests.
class StubPasswordManagerClient : public PasswordManagerClient {
 public:
  StubPasswordManagerClient();

  StubPasswordManagerClient(const StubPasswordManagerClient&) = delete;
  StubPasswordManagerClient& operator=(const StubPasswordManagerClient&) =
      delete;

  ~StubPasswordManagerClient() override;

  // PasswordManagerClient:
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool update_password) override;
  void PromptUserToMovePasswordToAccount(
      std::unique_ptr<PasswordFormManagerForUI> form_to_move) override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool update_password) override;
  void HideManualFallbackForSaving() override;
  void FocusedInputChanged(
      password_manager::PasswordManagerDriver* driver,
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin) override;
  void NotifyUserCouldBeAutoSignedIn(std::unique_ptr<PasswordForm>) override;
  void NotifySuccessfulLoginWithExistingPassword(
      std::unique_ptr<PasswordFormManagerForUI> submitted_manager) override;
  void NotifyStorePasswordCalled() override;
  void NotifyKeychainError() override;
  void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManagerForUI> saved_manager,
      bool is_update_confirmation) override;
  PrefService* GetPrefs() const override;
  PrefService* GetLocalStatePrefs() const override;
  const syncer::SyncService* GetSyncService() const override;
  affiliations::AffiliationService* GetAffiliationService() override;
  PasswordStoreInterface* GetProfilePasswordStore() const override;
  PasswordStoreInterface* GetAccountPasswordStore() const override;
  PasswordReuseManager* GetPasswordReuseManager() const override;
  const PasswordManagerInterface* GetPasswordManager() const override;
  const GURL& GetLastCommittedURL() const override;
  url::Origin GetLastCommittedOrigin() const override;
  const CredentialsFilter* GetStoreResultFilter() const override;
  autofill::LogManager* GetLogManager() override;
  const MockPasswordFeatureManager* GetPasswordFeatureManager() const override;
  MockPasswordFeatureManager* GetPasswordFeatureManager();
  version_info::Channel GetChannel() const override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
  void OpenPasswordDetailsBubble(
      const password_manager::PasswordForm& form) override;
  std::unique_ptr<
      password_manager::PasswordCrossDomainConfirmationPopupController>
  ShowCrossDomainConfirmationPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const GURL& domain,
      const std::u16string& password_origin,
      base::OnceClosure confirmation_callback) override;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;

#if defined(ON_FOCUS_PING_ENABLED)
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
#endif

  ukm::SourceId GetUkmSourceId() override;
  PasswordManagerMetricsRecorder* GetMetricsRecorder() override;
#if BUILDFLAG(IS_ANDROID)
  FirstCctPageLoadPasswordsUkmRecorder* GetFirstCctPageLoadUkmRecorder()
      override;
#endif
  signin::IdentityManager* GetIdentityManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::NetworkContext* GetNetworkContext() const override;
  bool IsIsolationForPasswordSitesEnabled() const override;
  bool IsNewTabPage() const override;

 private:
  const StubCredentialsFilter credentials_filter_;
  testing::NiceMock<MockPasswordFeatureManager> password_feature_manager_;
  autofill::StubLogManager log_manager_;
  ukm::SourceId ukm_source_id_;
  std::optional<PasswordManagerMetricsRecorder> metrics_recorder_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
