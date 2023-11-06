// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_prevent_silent_access_task.h"

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

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
    task_environment_.RunUntilIdle();
  }

 protected:
  testing::NiceMock<CredentialManagerPendingPreventSilentAccessTaskDelegateMock>
      delegate_mock_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest, ProfileStoreOnly) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(nullptr));

  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);

  task.AddOrigin(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    /*signon_realm=*/"www.example.com",
                                    GURL("www.example.com")));

  // We are expecting results from only one store, delegate should be called
  // upon getting a response from the store.
  EXPECT_CALL(delegate_mock_, DoneRequiringUserMediation);
  task.OnGetPasswordStoreResultsFrom(profile_store_.get(), {});
}

TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       ProfileAndAccountStores) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(account_store_.get()));

  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);

  task.AddOrigin(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    /*signon_realm=*/"www.example.com",
                                    GURL("www.example.com")));

  // We are expecting results from 2 stores, the delegate shouldn't be called
  // until both stores respond.
  EXPECT_CALL(delegate_mock_, DoneRequiringUserMediation).Times(0);
  task.OnGetPasswordStoreResultsFrom(profile_store_.get(), {});

  testing::Mock::VerifyAndClearExpectations(&delegate_mock_);

  EXPECT_CALL(delegate_mock_, DoneRequiringUserMediation);
  task.OnGetPasswordStoreResultsFrom(account_store_.get(), {});
}

}  // namespace password_manager
