// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_util.h"

#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_store/actionable_error.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Optional;
using ::testing::Return;

TEST(PasswordStoreUtilTest, JoinChangesPreservesEmptyAndUnknownResults) {
  EXPECT_THAT(JoinPasswordStoreChanges({}), ValueIs(Optional(IsEmpty())));
  EXPECT_THAT(JoinPasswordStoreChanges({PasswordStoreChangeList()}),
              ValueIs(Optional(IsEmpty())));
  EXPECT_THAT(JoinPasswordStoreChanges({std::nullopt}), ValueIs(std::nullopt));
  EXPECT_THAT(
      JoinPasswordStoreChanges({PasswordStoreChangeList(), std::nullopt}),
      ValueIs(std::nullopt));
}

TEST(PasswordStoreUtilTest, JoinChangesPreservesOrderAndInput) {
  const PasswordStoreChange added(PasswordStoreChange::ADD, StoredCredential());
  const PasswordStoreChange removed(PasswordStoreChange::REMOVE,
                                    StoredCredential());
  const std::vector<base::expected<std::optional<PasswordStoreChangeList>,
                                   PasswordStoreBackendError>>
      results = {PasswordStoreChangeList{added}, PasswordStoreChangeList(),
                 PasswordStoreChangeList{removed}};

  EXPECT_THAT(JoinPasswordStoreChanges(results),
              ValueIs(Optional(ElementsAre(added, removed))));
  EXPECT_THAT(results, ElementsAre(ValueIs(Optional(ElementsAre(added))),
                                   ValueIs(Optional(IsEmpty())),
                                   ValueIs(Optional(ElementsAre(removed)))));
}

TEST(PasswordStoreUtilTest, JoinChangesPreservesFirstErrorAndItsDetails) {
  PasswordStoreBackendError first_error(
      PasswordStoreBackendErrorType::kAuthErrorResolvable);
#if BUILDFLAG(IS_ANDROID)
  first_error.android_backend_api_error = 7;
#endif
  const PasswordStoreBackendError second_error(
      PasswordStoreBackendErrorType::kUncategorized);

  EXPECT_THAT(JoinPasswordStoreChanges({PasswordStoreChangeList(),
                                        base::unexpected(first_error),
                                        base::unexpected(second_error)}),
              ErrorIs(first_error));
}

TEST(PasswordStoreUtilTest, JoinChangesStopsAtFirstUnknownResultOrError) {
  const PasswordStoreBackendError error(
      PasswordStoreBackendErrorType::kUncategorized);
  EXPECT_THAT(JoinPasswordStoreChanges({std::nullopt, base::unexpected(error)}),
              ValueIs(std::nullopt));
  EXPECT_THAT(JoinPasswordStoreChanges({base::unexpected(error), std::nullopt}),
              ErrorIs(error));
}

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
