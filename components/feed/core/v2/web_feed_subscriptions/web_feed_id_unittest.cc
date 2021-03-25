// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

TEST(WebFeedId, ToStringFromStringIsIdentity) {
  EXPECT_EQ(WebFeedId(), WebFeedId::FromString(WebFeedId().ToString()));
  EXPECT_EQ(WebFeedId::FromWebFeedId("foo"),
            WebFeedId::FromString(WebFeedId::FromWebFeedId("foo").ToString()));
  EXPECT_EQ(WebFeedId::FromFollowId("bar"),
            WebFeedId::FromString(WebFeedId::FromFollowId("bar").ToString()));
}

TEST(WebFeedId, FromStringWithInvalidString) {
  EXPECT_EQ(WebFeedId(), WebFeedId::FromString(""));
  EXPECT_EQ(WebFeedId(), WebFeedId::FromString("blah"));
  EXPECT_EQ(WebFeedId(), WebFeedId::FromString("wfi"));
  EXPECT_EQ(WebFeedId(), WebFeedId::FromString("sub"));
  EXPECT_EQ(WebFeedId(), WebFeedId::FromString("sub:"));
  EXPECT_EQ(WebFeedId(), WebFeedId::FromString("wfi:"));
}

}  // namespace
}  // namespace feed
