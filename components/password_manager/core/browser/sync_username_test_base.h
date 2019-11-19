// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A base test fixture for mocking sync and signin infrastructure. Used for
// testing sync-related code.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_

#include <string>

#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class SyncUsernameTestBase : public testing::Test {
 public:
  SyncUsernameTestBase();
  ~SyncUsernameTestBase() override;

  // Instruct the identity manager to sign in with |email| or out.
  void FakeSigninAs(const std::string& email);

  // Produce a sample PasswordForm.
  static autofill::PasswordForm SimpleGaiaForm(const char* username);
  static autofill::PasswordForm SimpleNonGaiaForm(const char* username);
  static autofill::PasswordForm SimpleNonGaiaForm(const char* username,
                                                  const char* origin);

  // Instruct the sync service to pretend whether or not it is syncing
  // passwords.
  void SetSyncingPasswords(bool syncing_passwords);

  const syncer::SyncService* sync_service() const { return &sync_service_; }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

 private:
  base::test::TaskEnvironment scoped_task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_
