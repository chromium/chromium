// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data.h"

#include <optional>
#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

TEST(AccountPreviewDataTest, SerializeAndDeserialize) {
  AccountPreviewData data;
  data.password_count = 42;
  data.bookmark_count = 7;
  data.history_count = 3;
  data.password_domains = {"google.com", "yahoo.com"};

  base::DictValue serialized = AccountPreviewData::Serialize(data);

  std::optional<AccountPreviewData> deserialized =
      AccountPreviewData::Deserialize(base::Value(std::move(serialized)));
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(42, deserialized->password_count);
  EXPECT_EQ(7, deserialized->bookmark_count);
  EXPECT_EQ(3, deserialized->history_count);
  ASSERT_EQ(2U, deserialized->password_domains.size());
  EXPECT_EQ("google.com", deserialized->password_domains[0]);
  EXPECT_EQ("yahoo.com", deserialized->password_domains[1]);
}

TEST(AccountPreviewDataTest, DeserializeEmpty) {
  base::DictValue empty_dict;
  std::optional<AccountPreviewData> deserialized =
      AccountPreviewData::Deserialize(base::Value(std::move(empty_dict)));
  ASSERT_TRUE(deserialized.has_value());
  // Verify that empty dictionary results in default-initialized values.
  EXPECT_EQ(0, deserialized->password_count);
  EXPECT_EQ(0, deserialized->bookmark_count);
  EXPECT_EQ(0, deserialized->history_count);
  EXPECT_TRUE(deserialized->password_domains.empty());
}

TEST(AccountPreviewDataTest, DeserializeInvalidType) {
  base::Value int_value(42);
  std::optional<AccountPreviewData> deserialized =
      AccountPreviewData::Deserialize(int_value);
  EXPECT_FALSE(deserialized.has_value());
}

TEST(AccountPreviewDataTest, DeserializeJson) {
  std::string json = R"({
    "password_count": 42,
    "bookmark_count": 7,
    "history_count": 3,
    "password_domains": ["google.com", "yahoo.com"]
  })";
  std::optional<AccountPreviewData> deserialized =
      AccountPreviewData::Deserialize(json);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(42, deserialized->password_count);
  EXPECT_EQ(7, deserialized->bookmark_count);
  EXPECT_EQ(3, deserialized->history_count);
  ASSERT_EQ(2U, deserialized->password_domains.size());
  EXPECT_EQ("google.com", deserialized->password_domains[0]);
  EXPECT_EQ("yahoo.com", deserialized->password_domains[1]);
}

TEST(AccountPreviewDataTest, DeserializeInvalidJson) {
  std::string invalid_json = "{invalid json}";
  std::optional<AccountPreviewData> deserialized =
      AccountPreviewData::Deserialize(invalid_json);
  EXPECT_FALSE(deserialized.has_value());
}

}  // namespace signin
