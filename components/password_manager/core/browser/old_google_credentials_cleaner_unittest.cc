// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/old_google_credentials_cleaner.h"

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

PasswordForm CreateForm(std::string_view signon_realm) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.signon_realm = std::string(signon_realm);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

class MockCredentialsCleanerObserver : public CredentialsCleaner::Observer {
 public:
  MockCredentialsCleanerObserver() = default;
  ~MockCredentialsCleanerObserver() override = default;

  MOCK_METHOD(void, CleaningCompleted, (), (override));
};

}  // namespace

class OldGoogleCredentialCleanerTest : public testing::Test {
 public:
  OldGoogleCredentialCleanerTest() = default;
  ~OldGoogleCredentialCleanerTest() override = default;

  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(prefs::kWereOldGoogleLoginsRemoved,
                                           false);

    store_ = new testing::NiceMock<MockPasswordStoreInterface>;
  }

  void ExpectPasswords(std::vector<PasswordForm> password_forms) {
    EXPECT_CALL(*store_, GetAutofillableLogins)
        .WillOnce(testing::WithArg<0>(
            [password_forms, store = store_.get()](
                base::WeakPtr<PasswordStoreConsumer> consumer) {
              consumer->OnGetPasswordStoreResultsOrErrorFrom(
                  store, std::move(password_forms));
            }));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  MockPasswordStoreInterface* store() { return store_.get(); }
  TestingPrefServiceSimple& prefs() { return prefs_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  scoped_refptr<MockPasswordStoreInterface> store_;
};

// Tests that all old google.com accounts are deleted.
TEST_F(OldGoogleCredentialCleanerTest, TestOldGooglePasswordsAreDeleted) {
  ASSERT_FALSE(prefs().GetBoolean(prefs::kWereOldGoogleLoginsRemoved));

  std::vector<PasswordForm> forms = {
      CreateForm("http://www.google.com"),
      CreateForm("http://www.google.com/"),
      CreateForm("https://www.google.com"),
      CreateForm("https://www.google.com/"),
  };

  MockCredentialsCleanerObserver observer;
  OldGoogleCredentialCleaner cleaner{store(), &prefs()};
  ASSERT_TRUE(cleaner.NeedsCleaning());

  ExpectPasswords(forms);
  for (const auto& form : forms) {
    EXPECT_CALL(*store(), RemoveLogin(testing::_, form));
  }

  EXPECT_CALL(observer, CleaningCompleted);
  cleaner.StartCleaning(&observer);

  EXPECT_TRUE(prefs().GetBoolean(prefs::kWereOldGoogleLoginsRemoved));
}

// Tests that only old google.com accounts are deleted.
TEST_F(OldGoogleCredentialCleanerTest, TestNewerGooglePasswordsAreNotDeleted) {
  ASSERT_FALSE(prefs().GetBoolean(prefs::kWereOldGoogleLoginsRemoved));

  PasswordForm old_form = CreateForm("http://www.google.com");
  // Form created after cutoff.
  PasswordForm new_form = CreateForm("https://www.google.com");
  static constexpr base::Time::Exploded kTime = {
      .year = 2012, .month = 1, .day_of_month = 1, .second = 1};
  ASSERT_TRUE(base::Time::FromUTCExploded(kTime, &new_form.date_created));

  MockCredentialsCleanerObserver observer;
  OldGoogleCredentialCleaner cleaner{store(), &prefs()};
  ASSERT_TRUE(cleaner.NeedsCleaning());

  ExpectPasswords({old_form, new_form, CreateForm("http://test.com/")});
  EXPECT_CALL(*store(), RemoveLogin(testing::_, old_form));
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner.StartCleaning(&observer);

  EXPECT_TRUE(prefs().GetBoolean(prefs::kWereOldGoogleLoginsRemoved));
}

// Tests that clearing not needed if pref is true.
TEST_F(OldGoogleCredentialCleanerTest, NoClearingNeeded) {
  prefs().SetBoolean(prefs::kWereOldGoogleLoginsRemoved, true);

  OldGoogleCredentialCleaner cleaner{store(), &prefs()};
  EXPECT_FALSE(cleaner.NeedsCleaning());
}

}  // namespace password_manager
