// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_messaging/app_identifier.h"

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "components/push_messaging/app_identifier_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace push_messaging {

class AppIdentifierTest : public AppIdentifierTestSupport,
                          public testing::Test {};

TEST_F(AppIdentifierTest, ConstructorValidity) {
  // The following two are valid:
  EXPECT_FALSE(GenerateId("https://www.example.com/", 1).is_null());
  EXPECT_FALSE(GenerateId("https://www.example.com", 1).is_null());
  // The following four are invalid and will DCHECK in Generate:
  EXPECT_FALSE(GenerateId("", 1).is_null());
  EXPECT_FALSE(GenerateId("foo", 1).is_null());
  EXPECT_FALSE(GenerateId("https://www.example.com/foo", 1).is_null());
  EXPECT_FALSE(GenerateId("https://www.example.com/#foo", 1).is_null());
  // The following one is invalid and will DCHECK in Generate and be null:
  EXPECT_TRUE(GenerateId("https://www.example.com/", -1).is_null());
}

TEST_F(AppIdentifierTest, UniqueGuids) {
  EXPECT_NE(push_messaging::AppIdentifier::Generate(
                GURL("https://www.example.com/"), 1)
                .app_id(),
            push_messaging::AppIdentifier::Generate(
                GURL("https://www.example.com/"), 1)
                .app_id());
}

TEST_F(AppIdentifierTest, FromPrefValue) {
  auto first = push_messaging::AppIdentifier::Generate(
      GURL("https://www.example.com"), 1);
  auto second =
      AppIdentifier::FromPrefValue(first.app_id(), first.ToPrefValue());
  ASSERT_TRUE(second);
  ExpectAppIdentifiersEqual(first, *second);
}

TEST_F(AppIdentifierTest, FromPrefValueWithExpiration) {
  auto first = push_messaging::AppIdentifier::Generate(
      GURL("https://www.example.com"), 1,
      base::Time::FromSecondsSinceUnixEpoch(100));
  ASSERT_TRUE(first.IsExpired());
  auto second =
      AppIdentifier::FromPrefValue(first.app_id(), first.ToPrefValue());
  ASSERT_TRUE(second);
  ExpectAppIdentifiersEqual(first, *second);
}

// This test is failing due to the zero expiration time is explicitly
// disallowed. It's worth considering how it should be handled without crashing
// the process.
TEST_F(AppIdentifierTest, DISABLED_FromPrefValueWithZeroExpiration) {
  auto first = push_messaging::AppIdentifier::Generate(
      GURL("https://www.example.com"), 1,
      base::Time::FromSecondsSinceUnixEpoch(0));
  ASSERT_FALSE(first.IsExpired());
  auto second =
      AppIdentifier::FromPrefValue(first.app_id(), first.ToPrefValue());
  ASSERT_TRUE(second);
  ExpectAppIdentifiersEqual(first, *second);
}

TEST_F(AppIdentifierTest, FromPrefValueDifferentAppId) {
  auto first = push_messaging::AppIdentifier::Generate(
      GURL("https://www.example.com"), 1);
  auto second = AppIdentifier::FromPrefValue(
      GenerateId("https://www.not-example.com", 2).app_id(),
      first.ToPrefValue());
  ASSERT_TRUE(second);
  // Except for the app_id, other fields should be identical.
  ExpectAppIdentifiersEqual(first, ReplaceAppId(*second, first.app_id()));
}

TEST_F(AppIdentifierTest, FromPrefValueReturnsEmpty) {
  EXPECT_FALSE(AppIdentifier::FromPrefValue(
      GenerateId("http://www.example.com", 1).app_id(), "hey, yo!"));
}

}  // namespace push_messaging
