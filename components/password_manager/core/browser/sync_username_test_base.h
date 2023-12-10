// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A base test fixture for mocking sync and signin infrastructure. Used for
// testing sync-related code.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_

#include <string>

#include "base/test/task_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

struct PasswordForm;

class SyncUsernameTestBase : public testing::Test {
 public:
  SyncUsernameTestBase();
  ~SyncUsernameTestBase() override;

  // Instruct the identity manager to sign in with |email| or out and using
  // |consent_level|.
  void FakeSigninAs(const std::string& email,
                    signin::ConsentLevel consent_level);

  // Produce a sample PasswordForm.
  static PasswordForm SimpleGaiaForm(const char* username);
  static PasswordForm SimpleNonGaiaForm(const char* username);
  static PasswordForm SimpleNonGaiaForm(const char* username,
                                        const char* origin);

  // Instruct the sync service to pretend whether or not it is syncing
  // passwords.
  void SetSyncingPasswords(bool syncing_passwords);

  const syncer::SyncService* sync_service() const { return &sync_service_; }
  syncer::TestSyncService* test_sync_service() { return &sync_service_; }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  void FastForwardBy(base::TimeDelta delta) { task_env_.FastForwardBy(delta); }
  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_
