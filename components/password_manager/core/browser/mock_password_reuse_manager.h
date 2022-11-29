// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_REUSE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_REUSE_MANAGER_H_

#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
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
               PasswordStoreInterface* profile_store,
               PasswordStoreInterface* account_store),
              (override));
  MOCK_METHOD(void,
              ReportMetrics,
              (const std::string& username, bool is_under_advanced_protection),
              (override));
  MOCK_METHOD(void,
              PreparePasswordHashData,
              (const std::string& sync_username, bool is_signed_in),
              (override));
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
               GaiaPasswordHashChange event),
              (override));
  MOCK_METHOD(void,
              SaveEnterprisePasswordHash,
              (const std::string& username, const std::u16string& password),
              (override));
  MOCK_METHOD(void,
              SaveSyncPasswordHash,
              (const PasswordHashData& sync_password_data,
               GaiaPasswordHashChange event),
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
              SetPasswordStoreSigninNotifier,
              (std::unique_ptr<PasswordStoreSigninNotifier> notifier),
              (override));
  MOCK_METHOD(void,
              SchedulePasswordHashUpdate,
              (bool should_log_metrics,
               bool does_primary_account_exists,
               bool is_signed_in),
              (override));
  MOCK_METHOD(void, ScheduleEnterprisePasswordURLUpdate, (), (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_REUSE_MANAGER_H_
