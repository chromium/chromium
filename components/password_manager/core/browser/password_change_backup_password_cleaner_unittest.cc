// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_backup_password_cleaner.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
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

PasswordForm CreatePasswordForm(bool with_backup) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = u"username";
  form.password_value = u"password";
  if (with_backup) {
    form.SetPasswordBackupNote(u"backup");
  }
  return form;
}

void ExpectPasswordStoreReturns(const std::vector<PasswordForm>& forms,
                                MockPasswordStoreInterface* store) {
  EXPECT_CALL(*store, GetAutofillableLogins)
      .WillOnce(WithArg<0>([forms, store](
                               base::WeakPtr<PasswordStoreConsumer> consumer) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom,
                consumer, base::Unretained(store), std::move(forms)));
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

  ExpectPasswordStoreReturns({CreatePasswordForm(/*with_backup=*/true)},
                             store_ptr);

  // Advance the clock to ensure backup password's TTL passed.
  AdvanceClock(kBackupPasswordTTL);
  EXPECT_CALL(*store_ptr,
              UpdateLogin(CreatePasswordForm(/*with_backup=*/false), _));
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

  ExpectPasswordStoreReturns({CreatePasswordForm(/*with_backup=*/false)},
                             store_ptr);

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

  ExpectPasswordStoreReturns({CreatePasswordForm(/*with_backup=*/true)},
                             store_ptr);

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
