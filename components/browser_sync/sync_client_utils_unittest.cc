// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_client_utils.h"

#include <string>
#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

password_manager::PasswordForm CreateTestPassword(
    const std::string& url,
    password_manager::PasswordForm::Store store =
        password_manager::PasswordForm::Store::kProfileStore,
    base::Time last_used_time = base::Time::UnixEpoch()) {
  password_manager::PasswordForm form;
  form.signon_realm = url;
  form.url = GURL(url);
  form.username_value = u"test@gmail.com";
  form.password_value = u"test";
  form.in_store = store;
  form.date_last_used = last_used_time;
  return form;
}

class LocalDataQueryHelperTest : public testing::Test {
 public:
  LocalDataQueryHelperTest()
      : local_data_query_helper_(local_password_store_.get()) {
    local_password_store_->Init(/*prefs=*/nullptr,
                                /*affiliated_match_helper=*/nullptr);
  }
  ~LocalDataQueryHelperTest() override {
    local_password_store_->ShutdownOnUIThread();
  }
  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<password_manager::TestPasswordStore> local_password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  browser_sync::LocalDataQueryHelper local_data_query_helper_;
};

TEST_F(LocalDataQueryHelperTest, ShouldReturnLocalPasswordsViaCallback) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("https://www.amazon.de"));
  local_password_store_->AddLogin(
      CreateTestPassword("https://www.facebook.com"));

  RunAllPendingTasks();

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::PASSWORDS,
       syncer::LocalDataDescription(syncer::PASSWORDS, 2,
                                    {"amazon.de", "facebook.com"}, 2)}};

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_.Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                               callback.Get());
  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest, ShouldReturnCountOfDistinctDomains) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("https://www.amazon.de"));
  local_password_store_->AddLogin(
      CreateTestPassword("https://www.facebook.com"));
  // Another password with the same domain as an existing password.
  local_password_store_->AddLogin(
      CreateTestPassword("https://www.amazon.de/login"));

  RunAllPendingTasks();

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::PASSWORDS, syncer::LocalDataDescription(
                              syncer::PASSWORDS,
                              // Total passwords = 3.
                              /*item_count=*/3, {"amazon.de", "facebook.com"},
                              // Total distinct domains = 2.
                              /*domain_count=*/2)}};

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_.Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                               callback.Get());
  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest, ShouldHandleMultipleRequests) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("https://www.amazon.de"));
  local_password_store_->AddLogin(
      CreateTestPassword("https://www.facebook.com"));

  RunAllPendingTasks();

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback1;

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback2;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::PASSWORDS,
       syncer::LocalDataDescription(syncer::PASSWORDS, 2,
                                    {"amazon.de", "facebook.com"}, 2)}};

  // Request #1.
  EXPECT_CALL(callback1, Run(expected));
  local_data_query_helper_.Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                               callback1.Get());

  // Request #2.
  EXPECT_CALL(callback2, Run(expected));
  local_data_query_helper_.Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                               callback2.Get());

  RunAllPendingTasks();
}

// TODO(crbug.com/1451508): Implement the test when support for other types has
// been added.
TEST_F(LocalDataQueryHelperTest,
       ShouldOnlyTriggerCallbackWhenAllTypesHaveReturned) {}

class LocalDataMigrationHelperTest : public testing::Test {
 public:
  LocalDataMigrationHelperTest()
      : local_data_migration_helper_(local_password_store_.get(),
                                     account_password_store_.get()) {
    local_password_store_->Init(/*prefs=*/nullptr,
                                /*affiliated_match_helper=*/nullptr);
    account_password_store_->Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);
  }
  ~LocalDataMigrationHelperTest() override {
    local_password_store_->ShutdownOnUIThread();
    account_password_store_->ShutdownOnUIThread();
  }
  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<password_manager::TestPasswordStore> local_password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  scoped_refptr<password_manager::TestPasswordStore> account_password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore{true});
  browser_sync::LocalDataMigrationHelper local_data_migration_helper_;
};

TEST_F(LocalDataMigrationHelperTest, ShouldMovePasswordsToAccountStore) {
  // Add test data to local store.
  auto form1 = CreateTestPassword("https://www.amazon.de");
  auto form2 = CreateTestPassword("https://www.facebook.com");
  local_password_store_->AddLogin(form1);
  local_password_store_->AddLogin(form2);

  RunAllPendingTasks();

  ASSERT_EQ(
      local_password_store_->stored_passwords(),
      password_manager::TestPasswordStore::PasswordMap(
          {{form1.signon_realm, {form1}}, {form2.signon_realm, {form2}}}));

  local_data_migration_helper_.Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // Passwords have been moved to the account store.
  form1.in_store = password_manager::PasswordForm::Store::kAccountStore;
  form2.in_store = password_manager::PasswordForm::Store::kAccountStore;
  EXPECT_EQ(
      account_password_store_->stored_passwords(),
      password_manager::TestPasswordStore::PasswordMap(
          {{form1.signon_realm, {form1}}, {form2.signon_realm, {form2}}}));
  // Local password store is empty.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest, ShouldNotUploadSamePassword) {
  // Add test password to local store.
  auto local_form = CreateTestPassword("https://www.amazon.de");
  local_form.times_used_in_html_form = 10;
  local_password_store_->AddLogin(local_form);

  // Add the same password to the account store, with slight different
  // non-identifying details.
  auto account_form = local_form;
  account_form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  account_form.times_used_in_html_form = 5;
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  local_data_migration_helper_.Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // No new password is added to the account store.
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));
  // The password is removed from the local store.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldUploadConflictingPasswordIfMoreRecentlyUsed) {
  // Add test password to local store, with last used time set to (time for
  // epoch in Unix + 1 second).
  auto local_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore,
                         base::Time::UnixEpoch() + base::Seconds(1));
  local_form.password_value = u"local_value";
  local_password_store_->AddLogin(local_form);

  // Add same credential with a different password to the account store, with
  // last used time set to time for epoch in Unix.
  auto account_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore,
                         base::Time::UnixEpoch());
  account_form.password_value = u"account_value";
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  local_data_migration_helper_.Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // Since local password has a more recent last used date, it is moved to the
  // account store.
  local_form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  // Local password store is now empty.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldNotUploadConflictingPasswordIfLessRecentlyUsed) {
  // Add test password to local store, with last used time set to time for epoch
  // in Unix.
  auto local_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore,
                         base::Time::UnixEpoch());
  local_form.password_value = u"local_value";
  local_password_store_->AddLogin(local_form);

  // Add same credential with a different password to the account store, with
  // last used time set to (time for epoch in Unix + 1 second).
  auto account_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore,
                         base::Time::UnixEpoch() + base::Seconds(1));
  account_form.password_value = u"account_value";
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  local_data_migration_helper_.Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // Since account password has a more recent last used date, it wins over the
  // local password.
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {account_form}}}));
  // Local password is removed from the local store.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

// TODO(crbug.com/1451508): Implement the test with different types for
// different requests.
TEST_F(LocalDataMigrationHelperTest, ShouldHandleMultipleRequests) {}

}  // namespace
