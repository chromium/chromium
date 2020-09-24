// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_reader.h"

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::UnorderedElementsAre;

constexpr const char kTestWebRealm[] = "https://one.example.com/";

class MockCompromisedCredentialsReaderObserver
    : public CompromisedCredentialsReader::Observer {
 public:
  MOCK_METHOD(void,
              OnCompromisedCredentialsChanged,
              (const std::vector<CompromisedCredentials>&),
              (override));
};

class CompromisedCredentialsReaderTest : public ::testing::Test {
 protected:
  CompromisedCredentialsReaderTest() {
    profile_store_->Init(/*prefs=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr);
  }

  ~CompromisedCredentialsReaderTest() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }
  CompromisedCredentialsReader& reader() { return reader_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  CompromisedCredentialsReader reader_{profile_store_.get(),
                                       account_store_.get()};
};

}  // namespace

TEST_F(CompromisedCredentialsReaderTest, AddCredentialsToBothStores) {
  CompromisedCredentials profile_cred;
  profile_cred.signon_realm = kTestWebRealm;
  profile_cred.username = base::ASCIIToUTF16("profile@gmail.com");
  profile_cred.in_store = PasswordForm::Store::kProfileStore;

  CompromisedCredentials account_cred1;
  account_cred1.signon_realm = kTestWebRealm;
  account_cred1.username = base::ASCIIToUTF16("account1@gmail.com");
  account_cred1.in_store = PasswordForm::Store::kAccountStore;

  CompromisedCredentials account_cred2;
  account_cred2.signon_realm = kTestWebRealm;
  account_cred2.username = base::ASCIIToUTF16("account2@gmail.com");
  account_cred2.in_store = PasswordForm::Store::kAccountStore;

  ::testing::NiceMock<MockCompromisedCredentialsReaderObserver> mock_observer;
  reader().AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnCompromisedCredentialsChanged(
                                 UnorderedElementsAre(profile_cred)));
  profile_store().AddCompromisedCredentials(profile_cred);
  RunUntilIdle();

  EXPECT_CALL(mock_observer,
              OnCompromisedCredentialsChanged(
                  UnorderedElementsAre(profile_cred, account_cred1)));
  account_store().AddCompromisedCredentials(account_cred1);
  RunUntilIdle();

  EXPECT_CALL(mock_observer,
              OnCompromisedCredentialsChanged(UnorderedElementsAre(
                  profile_cred, account_cred1, account_cred2)));
  account_store().AddCompromisedCredentials(account_cred2);
  RunUntilIdle();

  EXPECT_CALL(mock_observer,
              OnCompromisedCredentialsChanged(
                  UnorderedElementsAre(account_cred1, account_cred2)));
  profile_store().RemoveCompromisedCredentials(
      profile_cred.signon_realm, profile_cred.username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();

  EXPECT_CALL(mock_observer,
              OnCompromisedCredentialsChanged(UnorderedElementsAre(
                  profile_cred, account_cred1, account_cred2)));
  profile_store().AddCompromisedCredentials(profile_cred);
  RunUntilIdle();

  reader().RemoveObserver(&mock_observer);
}

TEST_F(CompromisedCredentialsReaderTest, GetAllCompromisedCredentials) {
  CompromisedCredentials profile_cred;
  profile_cred.signon_realm = kTestWebRealm;
  profile_cred.username = base::ASCIIToUTF16("profile@gmail.com");
  profile_cred.in_store = PasswordForm::Store::kProfileStore;

  CompromisedCredentials account_cred;
  account_cred.signon_realm = kTestWebRealm;
  account_cred.username = base::ASCIIToUTF16("account1@gmail.com");
  account_cred.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddCompromisedCredentials(profile_cred);
  account_store().AddCompromisedCredentials(account_cred);

  base::MockCallback<
      CompromisedCredentialsReader::GetCompromisedCredentialsCallback>
      get_all_compromised_credentials_cb;

  reader().GetAllCompromisedCredentials(
      get_all_compromised_credentials_cb.Get());

  // The callback is run only after the stores respond in RunUntilIdle().
  EXPECT_CALL(get_all_compromised_credentials_cb,
              Run(UnorderedElementsAre(profile_cred, account_cred)));
  RunUntilIdle();
}

}  // namespace password_manager
