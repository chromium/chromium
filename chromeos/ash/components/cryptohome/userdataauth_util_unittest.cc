// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/userdataauth_util.h"

#include "chromeos/ash/components/cryptohome/common_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_data_auth {

namespace {

using ::cryptohome::KeyLabel;

constexpr char kKeyLabelStr[] = "key_label";

}  // namespace

class UserDataAuthUtilTest : public ::testing::Test {
 protected:
  const KeyLabel kKeyLabel = KeyLabel(kKeyLabelStr);
};

TEST_F(UserDataAuthUtilTest, ReplyToMountErrorNullOptional) {
  const std::optional<RemoveReply> reply = std::nullopt;
  EXPECT_EQ(ReplyToMountError(reply), cryptohome::MOUNT_ERROR_FATAL);
}

TEST_F(UserDataAuthUtilTest, ReplyToMountErrorNoError) {
  RemoveReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  const std::optional<RemoveReply> reply = std::move(result);
  EXPECT_EQ(ReplyToMountError(reply), cryptohome::MOUNT_ERROR_NONE);
}

TEST_F(UserDataAuthUtilTest, BaseReplyToMountErrorAuthFailure) {
  RemoveReply result;
  result.set_error(CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND);
  const std::optional<RemoveReply> reply = std::move(result);
  EXPECT_EQ(ReplyToMountError(reply), cryptohome::MOUNT_ERROR_KEY_FAILURE);
}

TEST_F(UserDataAuthUtilTest, CryptohomeErrorToMountError) {
  // Test a few commonly seen input output pair.
  EXPECT_EQ(CryptohomeErrorToMountError(CRYPTOHOME_ERROR_NOT_SET),
            cryptohome::MOUNT_ERROR_NONE);
  EXPECT_EQ(CryptohomeErrorToMountError(CRYPTOHOME_ERROR_MOUNT_FATAL),
            cryptohome::MOUNT_ERROR_FATAL);
  EXPECT_EQ(CryptohomeErrorToMountError(CRYPTOHOME_ERROR_TPM_DEFEND_LOCK),
            cryptohome::MOUNT_ERROR_TPM_DEFEND_LOCK);
  EXPECT_EQ(CryptohomeErrorToMountError(CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT),
            cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT);
}

TEST_F(UserDataAuthUtilTest, AccountDiskUsageReplyToUsageSizeNullOptional) {
  const std::optional<GetAccountDiskUsageReply> reply = std::nullopt;

  ASSERT_EQ(AccountDiskUsageReplyToUsageSize(reply), -1);
}

TEST_F(UserDataAuthUtilTest, AccountDiskUsageReplyToUsageSizeErrorInReply) {
  GetAccountDiskUsageReply result;
  result.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_IMPLEMENTED);
  const std::optional<GetAccountDiskUsageReply> reply = std::move(result);

  ASSERT_EQ(AccountDiskUsageReplyToUsageSize(reply), -1);
}

TEST_F(UserDataAuthUtilTest, AccountDiskUsageReplyToUsageSizeValidity) {
  constexpr int64_t kSize = 0x123456789ABCLL;
  GetAccountDiskUsageReply result;
  result.set_size(kSize);
  const std::optional<GetAccountDiskUsageReply> reply = std::move(result);

  ASSERT_EQ(AccountDiskUsageReplyToUsageSize(reply), kSize);
}

}  // namespace user_data_auth
