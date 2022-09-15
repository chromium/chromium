// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/old_google_credentials_cleaner.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

PasswordForm CreateForm(base::StringPiece signon_realm) {
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
              std::vector<std::unique_ptr<PasswordForm>> results;
              for (auto& form : password_forms)
                results.push_back(
                    std::make_unique<PasswordForm>(std::move(form)));
              consumer->OnGetPasswordStoreResultsOrErrorFrom(
                  store, std::move(results));
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

  OldGoogleCredentialCleaner cleaner{store(), &prefs()};
  ASSERT_TRUE(cleaner.NeedsCleaning());

  ExpectPasswords(forms);
  for (const auto& form : forms) {
    EXPECT_CALL(*store(), RemoveLogin(form));
  }

  MockCredentialsCleanerObserver observer;
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
  const base::Time::Exploded time = {2012, 1, 0, 1,
                                     0,    0, 0, 1};  // 00:01 Jan 1 2012
  ASSERT_TRUE(base::Time::FromUTCExploded(time, &new_form.date_created));

  OldGoogleCredentialCleaner cleaner{store(), &prefs()};
  ASSERT_TRUE(cleaner.NeedsCleaning());

  MockCredentialsCleanerObserver observer;
  ExpectPasswords({old_form, new_form, CreateForm("http://test.com/")});
  EXPECT_CALL(*store(), RemoveLogin(old_form));
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
