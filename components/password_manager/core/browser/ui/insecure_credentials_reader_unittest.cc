// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/insecure_credentials_reader.h"

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
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

class MockInsecureCredentialsReaderObserver
    : public InsecureCredentialsReader::Observer {
 public:
  MOCK_METHOD(void,
              OnInsecureCredentialsChanged,
              (const std::vector<InsecureCredential>&),
              (override));
};

PasswordForm MakeTestPassword(base::StringPiece username) {
  PasswordForm form;
  form.signon_realm = std::string(kTestWebRealm);
  form.url = GURL(kTestWebRealm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = u"password";
  form.username_element = u"username_element";
  return form;
}

class InsecureCredentialsReaderTest : public ::testing::Test {
 protected:
  InsecureCredentialsReaderTest() {
    profile_store_->Init(/*prefs=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr);
  }

  ~InsecureCredentialsReaderTest() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }
  InsecureCredentialsReader& reader() { return reader_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  InsecureCredentialsReader reader_{profile_store_.get(), account_store_.get()};
};

}  // namespace

TEST_F(InsecureCredentialsReaderTest, AddCredentialsToBothStores) {
  profile_store().AddLogin(MakeTestPassword("profile@gmail.com"));
  account_store().AddLogin(MakeTestPassword("account1@gmail.com"));
  account_store().AddLogin(MakeTestPassword("account2@gmail.com"));
  RunUntilIdle();

  InsecureCredential profile_cred;
  profile_cred.signon_realm = kTestWebRealm;
  profile_cred.username = u"profile@gmail.com";
  profile_cred.in_store = PasswordForm::Store::kProfileStore;

  InsecureCredential account_cred1;
  account_cred1.signon_realm = kTestWebRealm;
  account_cred1.username = u"account1@gmail.com";
  account_cred1.in_store = PasswordForm::Store::kAccountStore;

  InsecureCredential account_cred2;
  account_cred2.signon_realm = kTestWebRealm;
  account_cred2.username = u"account2@gmail.com";
  account_cred2.in_store = PasswordForm::Store::kAccountStore;

  ::testing::NiceMock<MockInsecureCredentialsReaderObserver> mock_observer;
  reader().AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer,
              OnInsecureCredentialsChanged(UnorderedElementsAre(profile_cred)));
  profile_store().AddInsecureCredential(profile_cred);
  RunUntilIdle();

  EXPECT_CALL(mock_observer, OnInsecureCredentialsChanged(UnorderedElementsAre(
                                 profile_cred, account_cred1)));
  account_store().AddInsecureCredential(account_cred1);
  RunUntilIdle();

  EXPECT_CALL(mock_observer, OnInsecureCredentialsChanged(UnorderedElementsAre(
                                 profile_cred, account_cred1, account_cred2)));
  account_store().AddInsecureCredential(account_cred2);
  RunUntilIdle();

  EXPECT_CALL(mock_observer, OnInsecureCredentialsChanged(UnorderedElementsAre(
                                 account_cred1, account_cred2)));
  profile_store().RemoveInsecureCredentials(
      profile_cred.signon_realm, profile_cred.username,
      RemoveInsecureCredentialsReason::kRemove);
  RunUntilIdle();

  EXPECT_CALL(mock_observer, OnInsecureCredentialsChanged(UnorderedElementsAre(
                                 profile_cred, account_cred1, account_cred2)));
  profile_store().AddInsecureCredential(profile_cred);
  RunUntilIdle();

  reader().RemoveObserver(&mock_observer);
}

TEST_F(InsecureCredentialsReaderTest, GetAllInsecureCredentials) {
  InsecureCredential profile_cred;
  profile_cred.signon_realm = kTestWebRealm;
  profile_cred.username = u"profile@gmail.com";
  profile_cred.in_store = PasswordForm::Store::kProfileStore;

  InsecureCredential account_cred;
  account_cred.signon_realm = kTestWebRealm;
  account_cred.username = u"account1@gmail.com";
  account_cred.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddInsecureCredential(profile_cred);
  account_store().AddInsecureCredential(account_cred);

  base::MockCallback<InsecureCredentialsReader::GetInsecureCredentialsCallback>
      get_all_insecure_credentials_cb;

  reader().GetAllInsecureCredentials(get_all_insecure_credentials_cb.Get());

  // The callback is run only after the stores respond in RunUntilIdle().
  EXPECT_CALL(get_all_insecure_credentials_cb,
              Run(UnorderedElementsAre(profile_cred, account_cred)));
  RunUntilIdle();
}

}  // namespace password_manager
