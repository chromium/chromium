// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_prevent_silent_access_task.h"

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr const char kUrl[] = "https://www.example.com";

class CredentialManagerPendingPreventSilentAccessTaskDelegateMock
    : public CredentialManagerPendingPreventSilentAccessTaskDelegate {
 public:
  CredentialManagerPendingPreventSilentAccessTaskDelegateMock() = default;
  ~CredentialManagerPendingPreventSilentAccessTaskDelegateMock() override =
      default;

  MOCK_METHOD(PasswordStoreInterface*, GetProfilePasswordStore, (), (override));
  MOCK_METHOD(PasswordStoreInterface*, GetAccountPasswordStore, (), (override));
  MOCK_METHOD(void, DoneRequiringUserMediation, (), (override));
};

}  // namespace

class CredentialManagerPendingPreventSilentAccessTaskTest
    : public ::testing::Test {
 public:
  CredentialManagerPendingPreventSilentAccessTaskTest() {
    profile_store_ = new TestPasswordStore(IsAccountStore(false));
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);

    account_store_ = new TestPasswordStore(IsAccountStore(true));
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
  }
  ~CredentialManagerPendingPreventSilentAccessTaskTest() override = default;

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    // It's needed to cleanup the password store asynchronously.
    ProcessPasswordStoreUpdates();
  }

  void ProcessPasswordStoreUpdates() { task_environment_.RunUntilIdle(); }

 protected:
  testing::NiceMock<CredentialManagerPendingPreventSilentAccessTaskDelegateMock>
      delegate_mock_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Verify that the delegate is notified when the credentials are fetched from
// the password store.
TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       NotifiesDelegate_ProfileStoreOnly) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(nullptr));

  // We are expecting results from only one store, delegate should be called
  // upon getting a response from the store.
  EXPECT_CALL(delegate_mock_, DoneRequiringUserMediation);

  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);
  task.AddOrigin(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    GetSignonRealm(GURL(kUrl)), GURL(kUrl)));
  ProcessPasswordStoreUpdates();
}

// Verify that the delegate is nofitied only once when credentials are fetched
// from both the profile and account password stores.
TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       NotifiesDelegate_ProfileAndAccountStores) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(account_store_.get()));

  // We are expecting results from 2 stores, the delegate should be called only
  // once after both stores return logins.
  EXPECT_CALL(delegate_mock_, DoneRequiringUserMediation);

  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);
  task.AddOrigin(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    GetSignonRealm(GURL(kUrl)), GURL(kUrl)));
  ProcessPasswordStoreUpdates();
}

}  // namespace password_manager
