// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/blacklisted_duplicates_cleaner.h"

#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class MockCredentialsCleanerObserver : public CredentialsCleaner::Observer {
 public:
  MockCredentialsCleanerObserver() = default;
  ~MockCredentialsCleanerObserver() override = default;
  MOCK_METHOD0(CleaningCompleted, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCredentialsCleanerObserver);
};

class BlacklistedDuplicatesCleanerTest : public ::testing::Test {
 public:
  BlacklistedDuplicatesCleanerTest() = default;

  ~BlacklistedDuplicatesCleanerTest() override = default;

 protected:
  TestPasswordStore* store() { return store_.get(); }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

  bool StoreContains(const autofill::PasswordForm& form) {
    const auto it = store_->stored_passwords().find(form.signon_realm);
    return it != store_->stored_passwords().end() &&
           base::ContainsValue(it->second, form);
  }

 private:
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  TestingPrefServiceSimple prefs_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistedDuplicatesCleanerTest);
};

TEST_F(BlacklistedDuplicatesCleanerTest, RemoveBlacklistedDuplicates) {
  base::test::ScopedTaskEnvironment scoped_task_environment;
  ASSERT_TRUE(
      store()->Init(syncer::SyncableService::StartSyncFlare(), nullptr));

  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = GURL("https://example.com/");
  blacklisted.signon_realm = "https://example.com/";
  store()->AddLogin(blacklisted);

  autofill::PasswordForm blacklisted_different;
  blacklisted_different.blacklisted_by_user = true;
  blacklisted_different.origin = GURL("https://host.com/");
  blacklisted_different.signon_realm = "https://host.com/";
  store()->AddLogin(blacklisted_different);

  autofill::PasswordForm blacklisted_duplicated;
  blacklisted_duplicated.blacklisted_by_user = true;
  blacklisted_duplicated.origin = GURL("https://example.com/duplicated/");
  blacklisted_duplicated.signon_realm = "https://example.com/";
  store()->AddLogin(blacklisted_duplicated);

  autofill::PasswordForm not_blacklisted;
  not_blacklisted.blacklisted_by_user = false;
  not_blacklisted.origin = GURL("https://google.com/");
  not_blacklisted.signon_realm = "https://google.com/";
  store()->AddLogin(not_blacklisted);

  scoped_task_environment.RunUntilIdle();

  // Check that all credentials were successfully added.
  ASSERT_TRUE(StoreContains(blacklisted));
  ASSERT_TRUE(StoreContains(blacklisted_different));
  ASSERT_TRUE(StoreContains(blacklisted_duplicated));
  ASSERT_TRUE(StoreContains(not_blacklisted));

  prefs()->registry()->RegisterBooleanPref(
      prefs::kDuplicatedBlacklistedCredentialsRemoved, false);

  // In this test we are explicitly only testing the clean up of duplicated
  // credentials and setting this true will prevent making another unrelated
  // clean-up.
  prefs()->registry()->RegisterBooleanPref(
      prefs::kCredentialsWithWrongSignonRealmRemoved, true);

  MockCredentialsCleanerObserver observer;
  auto cleaner =
      std::make_unique<BlacklistedDuplicatesCleaner>(store(), prefs());
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner->StartCleaning(&observer);
  scoped_task_environment.RunUntilIdle();

  // Check that one of the next two forms was removed.
  EXPECT_NE(StoreContains(blacklisted_duplicated), StoreContains(blacklisted));

  EXPECT_TRUE(StoreContains(blacklisted_different));
  EXPECT_TRUE(StoreContains(not_blacklisted));

  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kDuplicatedBlacklistedCredentialsRemoved));

  cleaner = std::make_unique<BlacklistedDuplicatesCleaner>(store(), prefs());
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner->StartCleaning(&observer);
  scoped_task_environment.RunUntilIdle();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kDuplicatedBlacklistedCredentialsRemoved));

  store()->ShutdownOnUIThread();
  scoped_task_environment.RunUntilIdle();
}

}  // namespace password_manager