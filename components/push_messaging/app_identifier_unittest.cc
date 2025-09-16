// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_messaging/app_identifier.h"

#include <stdint.h>

#include "base/time/time.h"
#include "components/push_messaging/app_identifier_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace push_messaging {

class AppIdentifierTest : public AppIdentifierTestSupport,
                          public testing::Test {};

TEST_F(AppIdentifierTest, ConstructorValidity) {
  // The following two are valid:
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/"), 1).is_null());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com"), 1).is_null());
  // The following four are invalid and will DCHECK in Generate:
  EXPECT_FALSE(GenerateId(GURL(""), 1).is_null());
  EXPECT_FALSE(GenerateId(GURL("foo"), 1).is_null());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/foo"), 1).is_null());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/#foo"), 1).is_null());
  // The following one is invalid and will DCHECK in Generate and be null:
  EXPECT_TRUE(GenerateId(GURL("https://www.example.com/"), -1).is_null());
}

TEST_F(AppIdentifierTest, UniqueGuids) {
  EXPECT_NE(push_messaging::AppIdentifier::Generate(
                GURL("https://www.example.com/"), 1)
                .app_id(),
            push_messaging::AppIdentifier::Generate(
                GURL("https://www.example.com/"), 1)
                .app_id());
}

}  // namespace push_messaging
