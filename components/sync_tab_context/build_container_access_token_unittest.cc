// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/build_container_access_token.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/tab_context_container_access_token.pb.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_tab_context {
namespace {

std::unique_ptr<syncer::AgileSymmetricKeySet> CreateTestKeySet() {
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  key_set->RotatePrimaryToNewlyGeneratedRandomKey();
  return key_set;
}

TEST(BuildContainerAccessTokenTest,
     ShouldBuildAccessTokenAndEncryptContainerKeySet) {
  std::unique_ptr<syncer::AgileSymmetricKeySet> container_key_set =
      CreateTestKeySet();
  std::unique_ptr<syncer::AgileSymmetricKeySet> ephemeral_key =
      CreateTestKeySet();
  std::unique_ptr<syncer::AgileSymmetricKeySet> ephemeral_key_for_decryption =
      syncer::AgileSymmetricKeySet::FromProto(ephemeral_key->ToProto());

  const base::Time kExpireTime = base::Time::UnixEpoch() +
                                 base::Seconds(1700000000) +
                                 base::Nanoseconds(123456000);

  EphemeralKeyFetcher::Result ephemeral_key_result;
  ephemeral_key_result.name = "ephemeralKeyInfos/key_1";
  ephemeral_key_result.ephemeral_key = std::move(ephemeral_key);
  ephemeral_key_result.expire_time = kExpireTime;

  const std::optional<std::string> serialized_token = BuildContainerAccessToken(
      *container_key_set, std::move(ephemeral_key_result));
  ASSERT_TRUE(serialized_token.has_value());

  sync_pb::TabContextContainerAccessToken token_proto;
  ASSERT_TRUE(token_proto.ParseFromString(*serialized_token));
  EXPECT_EQ(token_proto.name(), "ephemeralKeyInfos/key_1");
  ASSERT_TRUE(token_proto.has_expire_time());
  EXPECT_EQ(token_proto.expire_time().seconds(), 1700000000);
  EXPECT_EQ(token_proto.expire_time().nanos(), 123456000);

  sync_pb::EncryptedData encrypted_container_key;
  ASSERT_TRUE(encrypted_container_key.ParseFromString(
      token_proto.encrypted_container_key()));
  const std::optional<std::vector<uint8_t>> decrypted_container_key_bytes =
      ephemeral_key_for_decryption->Decrypt(encrypted_container_key);
  ASSERT_TRUE(decrypted_container_key_bytes.has_value());
  const std::string decrypted_container_key_str(
      decrypted_container_key_bytes->begin(),
      decrypted_container_key_bytes->end());
  EXPECT_EQ(decrypted_container_key_str,
            container_key_set->ToProto().SerializeAsString());
}

TEST(BuildContainerAccessTokenTest, ShouldOmitExpireTimeWhenNull) {
  std::unique_ptr<syncer::AgileSymmetricKeySet> container_key_set =
      CreateTestKeySet();
  std::unique_ptr<syncer::AgileSymmetricKeySet> ephemeral_key =
      CreateTestKeySet();

  EphemeralKeyFetcher::Result ephemeral_key_result;
  ephemeral_key_result.name = "ephemeralKeyInfos/key_1";
  ephemeral_key_result.ephemeral_key = std::move(ephemeral_key);
  ephemeral_key_result.expire_time = base::Time();

  const std::optional<std::string> serialized_token = BuildContainerAccessToken(
      *container_key_set, std::move(ephemeral_key_result));
  ASSERT_TRUE(serialized_token.has_value());

  sync_pb::TabContextContainerAccessToken token_proto;
  ASSERT_TRUE(token_proto.ParseFromString(*serialized_token));
  EXPECT_EQ(token_proto.name(), "ephemeralKeyInfos/key_1");
  EXPECT_FALSE(token_proto.has_expire_time());
}

TEST(BuildContainerAccessTokenTest, ShouldReturnNulloptWhenEncryptionFails) {
  std::unique_ptr<syncer::AgileSymmetricKeySet> container_key_set =
      CreateTestKeySet();

  EphemeralKeyFetcher::Result ephemeral_key_result;
  ephemeral_key_result.name = "ephemeralKeyInfos/key_1";
  ephemeral_key_result.ephemeral_key =
      syncer::AgileSymmetricKeySet::CreateEmpty();

  EXPECT_EQ(BuildContainerAccessToken(*container_key_set,
                                      std::move(ephemeral_key_result)),
            std::nullopt);
}

}  // namespace
}  // namespace sync_tab_context
