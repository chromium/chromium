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

constexpr int64_t kKeyRevision = 123;
constexpr char kProviderData1Name[] = "data_1";
constexpr int64_t kProviderData1Number = 12345;
constexpr char kProviderData2Name[] = "data_2";
constexpr char kProviderData2Bytes[] = "data_2 bytes";

}  // namespace

class UserDataAuthUtilTest : public ::testing::Test {
 protected:
  const KeyLabel kKeyLabel = KeyLabel(kKeyLabelStr);
};

TEST_F(UserDataAuthUtilTest, ReplyToMountErrorNullOptional) {
  const absl::optional<RemoveReply> reply = absl::nullopt;
  EXPECT_EQ(ReplyToMountError(reply), cryptohome::MOUNT_ERROR_FATAL);
}

TEST_F(UserDataAuthUtilTest, ReplyToMountErrorNoError) {
  RemoveReply result;
  result.set_error(CRYPTOHOME_ERROR_NOT_SET);
  const absl::optional<RemoveReply> reply = std::move(result);
  EXPECT_EQ(ReplyToMountError(reply), cryptohome::MOUNT_ERROR_NONE);
}

TEST_F(UserDataAuthUtilTest, BaseReplyToMountErrorAuthFailure) {
  RemoveReply result;
  result.set_error(CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND);
  const absl::optional<RemoveReply> reply = std::move(result);
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

TEST_F(UserDataAuthUtilTest, GetKeyDataReplyToKeyDefinitionsTwoEntries) {
  GetKeyDataReply result;
  result.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET);
  cryptohome::KeyData* key_data = result.add_key_data();
  key_data->set_type(cryptohome::KeyData::KEY_TYPE_PASSWORD);
  key_data->set_label(kKeyLabelStr);
  key_data->mutable_privileges()->set_update(false);
  key_data->set_revision(kKeyRevision);
  cryptohome::KeyProviderData* data = key_data->mutable_provider_data();
  cryptohome::KeyProviderData::Entry* entry1 = data->add_entry();
  entry1->set_name(kProviderData1Name);
  entry1->set_number(kProviderData1Number);
  cryptohome::KeyProviderData::Entry* entry2 = data->add_entry();
  entry2->set_name(kProviderData2Name);
  entry2->set_bytes(kProviderData2Bytes);
  const absl::optional<GetKeyDataReply> reply = std::move(result);

  std::vector<cryptohome::KeyDefinition> key_definitions =
      GetKeyDataReplyToKeyDefinitions(reply);

  // Verify that the call was successful and the result was correctly parsed.
  ASSERT_EQ(1u, key_definitions.size());
  const cryptohome::KeyDefinition& key_definition = key_definitions.front();
  EXPECT_EQ(cryptohome::KeyDefinition::TYPE_PASSWORD, key_definition.type);
  EXPECT_EQ(kKeyLabel, key_definition.label);
  EXPECT_EQ(cryptohome::PRIV_ADD | cryptohome::PRIV_REMOVE,
            key_definition.privileges);
  EXPECT_EQ(kKeyRevision, key_definition.revision);
  ASSERT_EQ(2u, key_definition.provider_data.size());
  const cryptohome::KeyDefinition::ProviderData* provider_data =
      &key_definition.provider_data[0];
  EXPECT_EQ(kProviderData1Name, provider_data->name);
  ASSERT_TRUE(provider_data->number);
  EXPECT_EQ(kProviderData1Number, *provider_data->number.get());
  EXPECT_FALSE(provider_data->bytes);
  provider_data = &key_definition.provider_data[1];
  EXPECT_EQ(kProviderData2Name, provider_data->name);
  EXPECT_FALSE(provider_data->number);
  ASSERT_TRUE(provider_data->bytes);
  EXPECT_EQ(kProviderData2Bytes, *provider_data->bytes.get());
}

// Test the GetKeyDataReplyToKeyDefinitions() function against the
// GetKeyDataReply proto containing the KeyData proto of the
// |KEY_TYPE_CHALLENGE_RESPONSE| type.
TEST_F(UserDataAuthUtilTest,
       GetKeyDataReplyToKeyDefinitions_ChallengeResponse) {
  GetKeyDataReply result;
  result.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET);
  cryptohome::KeyData* key_data = result.add_key_data();
  key_data->set_type(cryptohome::KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  key_data->set_label(kKeyLabelStr);
  const absl::optional<GetKeyDataReply> reply = std::move(result);

  const std::vector<cryptohome::KeyDefinition> key_definitions =
      GetKeyDataReplyToKeyDefinitions(reply);

  ASSERT_EQ(1u, key_definitions.size());
  const cryptohome::KeyDefinition& key_definition = key_definitions.front();
  EXPECT_EQ(cryptohome::KeyDefinition::TYPE_CHALLENGE_RESPONSE,
            key_definition.type);
  EXPECT_EQ(kKeyLabel, key_definition.label);
}

TEST_F(UserDataAuthUtilTest, AccountDiskUsageReplyToUsageSizeNullOptional) {
  const absl::optional<GetAccountDiskUsageReply> reply = absl::nullopt;

  ASSERT_EQ(AccountDiskUsageReplyToUsageSize(reply), -1);
}

TEST_F(UserDataAuthUtilTest, AccountDiskUsageReplyToUsageSizeErrorInReply) {
  GetAccountDiskUsageReply result;
  result.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_IMPLEMENTED);
  const absl::optional<GetAccountDiskUsageReply> reply = std::move(result);

  ASSERT_EQ(AccountDiskUsageReplyToUsageSize(reply), -1);
}

TEST_F(UserDataAuthUtilTest, AccountDiskUsageReplyToUsageSizeValidity) {
  constexpr int64_t kSize = 0x123456789ABCLL;
  GetAccountDiskUsageReply result;
  result.set_size(kSize);
  const absl::optional<GetAccountDiskUsageReply> reply = std::move(result);

  ASSERT_EQ(AccountDiskUsageReplyToUsageSize(reply), kSize);
}

}  // namespace user_data_auth
