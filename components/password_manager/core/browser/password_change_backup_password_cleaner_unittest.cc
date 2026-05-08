// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_backup_password_cleaner.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::password_manager::kBackupPasswordCleaningDelay;
using ::password_manager::kBackupPasswordTTL;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Values;
using ::testing::WithArg;

std::string_view GetStorePref(IsAccountStore is_account_store) {
  return is_account_store
             ? prefs::kAccountStoreBackupPasswordCleaningLastTimestamp
             : prefs::kProfileStoreBackupPasswordCleaningLastTimestamp;
}

StoredCredential CreateStoredCredential(bool with_backup) {
  StoredCredential cred;
  cred.signon_realm = "https://example.com";
  cred.username_value = u"username";
  cred.password_value = u"password";
  if (with_backup) {
    cred.notes.emplace_back(PasswordNote::kPasswordChangeBackupNoteName,
                            u"backup", base::Time::Now(), false);
  }
  return cred;
}

void ExpectPasswordStoreReturns(std::vector<StoredCredential> credentials,
                                MockPasswordStoreInterface* store) {
  EXPECT_CALL(*store, GetAutofillableLogins)
      .WillOnce(WithArg<0>([credentials = std::move(credentials),
                            store](base::WeakPtr<PasswordStoreConsumer>
                                       consumer) mutable {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom,
                consumer, base::Unretained(store), std::move(credentials)));
      }));
}

class MockCredentialsCleanerObserver : public CredentialsCleaner::Observer {
 public:
  MockCredentialsCleanerObserver() = default;
  ~MockCredentialsCleanerObserver() override = default;

  MOCK_METHOD(void, CleaningCompleted, (), (override));
};

class PasswordChangeBackupPasswordCleanerTest
    : public testing::Test,
      public testing::WithParamInterface<IsAccountStore> {
 public:
  PasswordChangeBackupPasswordCleanerTest() {
    prefs_.registry()->RegisterTimePref(GetStorePref(GetParam()),
                                        base::Time::Now());
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple prefs_;
};

TEST_P(PasswordChangeBackupPasswordCleanerTest,
       CleaningNotNeededWhenDelayNotPassed) {
  PasswordChangeBackupPasswordCleaner cleaner(GetParam(), /*store=*/nullptr,
                                              prefs());

  AdvanceClock(kBackupPasswordCleaningDelay - base::Minutes(5));
  EXPECT_FALSE(cleaner.NeedsCleaning());
}

TEST_P(PasswordChangeBackupPasswordCleanerTest, CleaningNeededWhenDelayPassed) {
  PasswordChangeBackupPasswordCleaner cleaner(GetParam(), /*store=*/nullptr,
                                              prefs());

  AdvanceClock(kBackupPasswordCleaningDelay);
  EXPECT_TRUE(cleaner.NeedsCleaning());
}

TEST_P(PasswordChangeBackupPasswordCleanerTest,
       UpdatesPasswordWithBackupThatReachedTTL) {
  scoped_refptr<MockPasswordStoreInterface> store =
      base::MakeRefCounted<NiceMock<MockPasswordStoreInterface>>();
  MockPasswordStoreInterface* store_ptr = store.get();
  PasswordChangeBackupPasswordCleaner cleaner(GetParam(), std::move(store),
                                              prefs());

  std::vector<password_manager::StoredCredential> forms;
  forms.push_back(CreateStoredCredential(/*with_backup=*/true));
  ExpectPasswordStoreReturns(std::move(forms), store_ptr);

  // Advance the clock to ensure backup password's TTL passed.
  AdvanceClock(kBackupPasswordTTL);
  EXPECT_CALL(*store_ptr,
              UpdateLogin(EqStoredCredential(password_manager::ToPasswordForm(
                              CreateStoredCredential(/*with_backup=*/false))),
                          _));
  MockCredentialsCleanerObserver observer;
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner.StartCleaning(&observer);
  WaitForPasswordStore();

  EXPECT_EQ(prefs()->GetTime(GetStorePref(GetParam())), base::Time::Now());
}

TEST_P(PasswordChangeBackupPasswordCleanerTest,
       DoesNotUpdatePasswordWithoutBackup) {
  scoped_refptr<MockPasswordStoreInterface> store =
      base::MakeRefCounted<NiceMock<MockPasswordStoreInterface>>();
  MockPasswordStoreInterface* store_ptr = store.get();
  PasswordChangeBackupPasswordCleaner cleaner(GetParam(), std::move(store),
                                              prefs());

  std::vector<password_manager::StoredCredential> forms;
  forms.push_back(CreateStoredCredential(/*with_backup=*/false));
  ExpectPasswordStoreReturns(std::move(forms), store_ptr);

  // Advance the clock to ensure backup password's TTL passed.
  AdvanceClock(kBackupPasswordTTL);
  EXPECT_CALL(*store_ptr, UpdateLogin).Times(0);
  MockCredentialsCleanerObserver observer;
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner.StartCleaning(&observer);
  WaitForPasswordStore();

  EXPECT_EQ(prefs()->GetTime(GetStorePref(GetParam())), base::Time::Now());
}

TEST_P(PasswordChangeBackupPasswordCleanerTest,
       DoesNotUpdatePasswordWithBackupThatDidNotReachTTL) {
  scoped_refptr<MockPasswordStoreInterface> store =
      base::MakeRefCounted<NiceMock<MockPasswordStoreInterface>>();
  MockPasswordStoreInterface* store_ptr = store.get();
  PasswordChangeBackupPasswordCleaner cleaner(GetParam(), std::move(store),
                                              prefs());

  std::vector<password_manager::StoredCredential> forms;
  forms.push_back(CreateStoredCredential(/*with_backup=*/true));
  ExpectPasswordStoreReturns(std::move(forms), store_ptr);

  // Advance time to right before hitting the TTL.
  AdvanceClock(kBackupPasswordTTL - base::Minutes(5));
  EXPECT_CALL(*store_ptr, UpdateLogin).Times(0);
  MockCredentialsCleanerObserver observer;
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner.StartCleaning(&observer);
  WaitForPasswordStore();

  EXPECT_EQ(prefs()->GetTime(GetStorePref(GetParam())), base::Time::Now());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PasswordChangeBackupPasswordCleanerTest,
                         Values(kAccountStore, kProfileStore));

}  // namespace
}  // namespace password_manager
