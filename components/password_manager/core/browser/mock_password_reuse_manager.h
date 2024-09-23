// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_REUSE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_REUSE_MANAGER_H_

#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_reuse_manager_signin_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

// Mocked PasswordReuseManager used by unit tests.
class MockPasswordReuseManager : public PasswordReuseManager {
 public:
  MockPasswordReuseManager();
  ~MockPasswordReuseManager() override;
  MOCK_METHOD(void,
              Init,
              (PrefService * prefs,
               PrefService* local_prefs,
               PasswordStoreInterface* profile_store,
               PasswordStoreInterface* account_store,
               std::unique_ptr<PasswordReuseDetector> password_reuse_detector,
               signin::IdentityManager* identity_manager,
               std::unique_ptr<SharedPreferencesDelegate> shared_pref_delegate),
              (override));
  MOCK_METHOD(void, ReportMetrics, (const std::string& username), (override));
  MOCK_METHOD(void,
              CheckReuse,
              (const std::u16string& input,
               const std::string& domain,
               PasswordReuseDetectorConsumer* consumer),
              (override));
  MOCK_METHOD(void,
              SaveGaiaPasswordHash,
              (const std::string& username,
               const std::u16string& password,
               bool is_primary_account,
               metrics_util::GaiaPasswordHashChange event),
              (override));
  MOCK_METHOD(void,
              SaveEnterprisePasswordHash,
              (const std::string& username, const std::u16string& password),
              (override));
  MOCK_METHOD(void,
              SaveSyncPasswordHash,
              (const PasswordHashData& sync_password_data,
               metrics_util::GaiaPasswordHashChange event),
              (override));
  MOCK_METHOD(void,
              ClearGaiaPasswordHash,
              (const std::string& username),
              (override));
  MOCK_METHOD(void, ClearAllGaiaPasswordHash, (), (override));
  MOCK_METHOD(void, ClearAllEnterprisePasswordHash, (), (override));

  MOCK_METHOD(void, ClearAllNonGmailPasswordHash, (), (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterStateCallbackOnHashPasswordManager,
              (const base::RepeatingCallback<void(const std::string& username)>&
                   callback),
              (override));
  MOCK_METHOD(void,
              SetPasswordReuseManagerSigninNotifier,
              (std::unique_ptr<PasswordReuseManagerSigninNotifier> notifier),
              (override));
  MOCK_METHOD(void, ScheduleEnterprisePasswordURLUpdate, (), (override));
  MOCK_METHOD(void,
              MaybeSavePasswordHash,
              (const PasswordForm* submitted_form,
               PasswordManagerClient* client),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_REUSE_MANAGER_H_
