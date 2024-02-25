// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/webcontents_interaction_test_util.h"

#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(WebContentsInteractionTestUtilTest, IsTruthy) {
  EXPECT_FALSE(WebContentsInteractionTestUtil::IsTruthy(base::Value()));
  EXPECT_FALSE(WebContentsInteractionTestUtil::IsTruthy(base::Value(0)));
  EXPECT_TRUE(WebContentsInteractionTestUtil::IsTruthy(base::Value(3)));
  EXPECT_FALSE(WebContentsInteractionTestUtil::IsTruthy(base::Value(0.0)));
  EXPECT_TRUE(WebContentsInteractionTestUtil::IsTruthy(base::Value(3.0)));
  EXPECT_FALSE(WebContentsInteractionTestUtil::IsTruthy(base::Value("")));
  EXPECT_TRUE(WebContentsInteractionTestUtil::IsTruthy(base::Value("abc")));
  // Even empty lists and objects/dictionaries are truthy.
  EXPECT_TRUE(WebContentsInteractionTestUtil::IsTruthy(
      base::Value(base::Value::List())));
  EXPECT_TRUE(WebContentsInteractionTestUtil::IsTruthy(
      base::Value(base::Value::Dict())));
}

TEST(WebContentsInteractionTestUtilTest, DeepQueryAddSegment) {
  using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
  DeepQuery query{"a", "b"};
  DeepQuery combined_query = query + "c";
  std::vector<std::string> query_contents(combined_query.begin(),
                                          combined_query.end());
  EXPECT_THAT(query_contents, testing::ElementsAre("a", "b", "c"));

  // Add multiple segments.
  DeepQuery combined_query_2 = query + "c" + "d" + "e";
  query_contents = {combined_query_2.begin(), combined_query_2.end()};
  EXPECT_THAT(query_contents, testing::ElementsAre("a", "b", "c", "d", "e"));
}
