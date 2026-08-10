// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_util.h"

#include "components/password_manager/core/browser/password_store/actionable_error.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::Return;

TEST(PasswordStoreUtilTest, AccountStoreError) {
  auto account_store =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();
  auto profile_store =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();

  EXPECT_CALL(*account_store, GetError())
      .WillRepeatedly(Return(ActionableError::kTrustedVaultKeyNeeded));
  EXPECT_CALL(*profile_store, GetError())
      .WillRepeatedly(Return(ActionableError::kNoError));

  EXPECT_EQ(ActionableError::kTrustedVaultKeyNeeded,
            GetActionableErrorFromPasswordStores(account_store.get(),
                                                 profile_store.get()));
}

TEST(PasswordStoreUtilTest, ProfileStoreError) {
  auto account_store =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();
  auto profile_store =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();

  EXPECT_CALL(*account_store, GetError())
      .WillRepeatedly(Return(ActionableError::kNoError));
  EXPECT_CALL(*profile_store, GetError())
      .WillRepeatedly(Return(ActionableError::kTrustedVaultKeyNeeded));

  // Account store is clean.
  EXPECT_EQ(ActionableError::kTrustedVaultKeyNeeded,
            GetActionableErrorFromPasswordStores(account_store.get(),
                                                 profile_store.get()));

  // Account store is null.
  EXPECT_EQ(ActionableError::kTrustedVaultKeyNeeded,
            GetActionableErrorFromPasswordStores(nullptr, profile_store.get()));
}

TEST(PasswordStoreUtilTest, AccountStorePrecedence) {
  auto account_store =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();
  auto profile_store =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();

  EXPECT_CALL(*account_store, GetError())
      .WillRepeatedly(Return(ActionableError::kTrustedVaultKeyNeeded));
  EXPECT_CALL(*profile_store, GetError())
      .WillRepeatedly(Return(ActionableError::kSignInNeeded));

  EXPECT_EQ(ActionableError::kTrustedVaultKeyNeeded,
            GetActionableErrorFromPasswordStores(account_store.get(),
                                                 profile_store.get()));

  EXPECT_CALL(*account_store, GetError())
      .WillRepeatedly(Return(ActionableError::kSignInNeeded));
  EXPECT_CALL(*profile_store, GetError())
      .WillRepeatedly(Return(ActionableError::kTrustedVaultKeyNeeded));

  EXPECT_EQ(ActionableError::kSignInNeeded,
            GetActionableErrorFromPasswordStores(account_store.get(),
                                                 profile_store.get()));
}

TEST(PasswordStoreUtilTest, NoError) {
  auto account_store =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();
  auto profile_store =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();

  EXPECT_CALL(*account_store, GetError())
      .WillRepeatedly(Return(ActionableError::kNoError));
  EXPECT_CALL(*profile_store, GetError())
      .WillRepeatedly(Return(ActionableError::kNoError));

  EXPECT_EQ(ActionableError::kNoError,
            GetActionableErrorFromPasswordStores(account_store.get(),
                                                 profile_store.get()));
  EXPECT_EQ(ActionableError::kNoError,
            GetActionableErrorFromPasswordStores(nullptr, nullptr));
}

}  // namespace
}  // namespace password_manager
