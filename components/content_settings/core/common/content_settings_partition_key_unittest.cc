// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_partition_key.h"

#include <optional>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

TEST(PartitionKey, KeysAreDifferentIfInMemoryIsDifferent) {
  PartitionKey key1 = PartitionKey::CreateForTesting(
      /*domain=*/"foo", /*name=*/"bar", /*in_memory=*/false);
  PartitionKey key2 = PartitionKey::CreateForTesting(
      /*domain=*/"foo", /*name=*/"bar", /*in_memory=*/true);
  EXPECT_EQ(key1, key1);
  EXPECT_EQ(key1.Serialize(), key1.Serialize());
  EXPECT_NE(key1, key2);
  EXPECT_NE(key1.Serialize(), key2.Serialize());
}

TEST(PartitionKey, SerializeAndDeserialize) {
  auto serialize_and_deserialize = [](const PartitionKey& key) {
    auto serialized = key.Serialize();
    auto deserialized = PartitionKey::Deserialize(serialized);
    EXPECT_EQ(deserialized, std::make_optional(key));
  };

  serialize_and_deserialize(PartitionKey::GetDefaultForTesting());
  serialize_and_deserialize(PartitionKey::CreateForTesting(
      /*domain=*/"foo", /*name=*/"bar", /*in_memory=*/false));
  serialize_and_deserialize(PartitionKey::CreateForTesting(
      /*domain=*/"hello", /*name=*/"world", /*in_memory=*/true));

  EXPECT_EQ(PartitionKey::Deserialize("[\"hello\",\"world\",true]"),
            std::make_optional(PartitionKey::CreateForTesting(
                /*domain=*/"hello", /*name=*/"world", /*in_memory=*/true)));
  EXPECT_EQ(PartitionKey::Deserialize(""), std::nullopt);
  EXPECT_EQ(PartitionKey::Deserialize("true"), std::nullopt);
  EXPECT_EQ(PartitionKey::Deserialize("[\"hello\",\"world\",1]"), std::nullopt);
  EXPECT_EQ(PartitionKey::Deserialize("[\"hello\",\"world\"]"), std::nullopt);
  EXPECT_EQ(PartitionKey::Deserialize("[]"), std::nullopt);
  EXPECT_EQ(PartitionKey::Deserialize("[1]"), std::nullopt);
  EXPECT_EQ(PartitionKey::Deserialize("[\"hello\",\"world\", true]"),
            std::nullopt)
      << "non-canonical serialized keys are rejected";
}

}  // namespace content_settings
