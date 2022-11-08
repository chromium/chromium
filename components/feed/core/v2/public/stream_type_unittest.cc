// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/stream_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

class StreamTypeTest : public testing::Test {
 public:
  StreamTypeTest() = default;
  StreamTypeTest(StreamTypeTest&) = delete;
  StreamTypeTest& operator=(const StreamTypeTest&) = delete;
  ~StreamTypeTest() override = default;

  void SetUp() override { testing::Test::SetUp(); }
};

TEST_F(StreamTypeTest, ComparatorTest) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType following = StreamType(StreamKind::kFollowing);
  StreamType single_web_feed_a = StreamType(StreamKind::kSingleWebFeed, "A");
  StreamType single_web_feed_b = StreamType(StreamKind::kSingleWebFeed, "B");
  StreamType single_web_feed = StreamType(StreamKind::kSingleWebFeed);

  ASSERT_TRUE(for_you < following);
  ASSERT_TRUE(following < single_web_feed_a);
  ASSERT_TRUE(single_web_feed_a < single_web_feed_b);
  ASSERT_TRUE(for_you < single_web_feed_b);
  ASSERT_TRUE(single_web_feed_b == single_web_feed_b);
  ASSERT_FALSE(single_web_feed_a < single_web_feed);
  ASSERT_FALSE(single_web_feed < for_you);
  ASSERT_FALSE(for_you < for_you);
  ASSERT_FALSE(single_web_feed_b < single_web_feed_b);
}

TEST_F(StreamTypeTest, IdentityTest) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType following = StreamType(StreamKind::kFollowing);
  StreamType single_web_feed = StreamType(StreamKind::kSingleWebFeed);
  StreamType unknown = StreamType();

  ASSERT_TRUE(for_you.IsForYou());
  ASSERT_FALSE(for_you.IsWebFeed());
  ASSERT_FALSE(for_you.IsSingleWebFeed());
  ASSERT_TRUE(for_you.IsValid());

  ASSERT_FALSE(following.IsForYou());
  ASSERT_TRUE(following.IsWebFeed());
  ASSERT_FALSE(following.IsSingleWebFeed());
  ASSERT_TRUE(following.IsValid());

  ASSERT_FALSE(single_web_feed.IsForYou());
  ASSERT_FALSE(single_web_feed.IsWebFeed());
  ASSERT_TRUE(single_web_feed.IsSingleWebFeed());
  ASSERT_TRUE(single_web_feed.IsValid());

  ASSERT_FALSE(unknown.IsForYou());
  ASSERT_FALSE(unknown.IsWebFeed());
  ASSERT_FALSE(unknown.IsSingleWebFeed());
  ASSERT_FALSE(unknown.IsValid());
}

TEST_F(StreamTypeTest, StringTest) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType following = StreamType(StreamKind::kFollowing);
  StreamType single_web_feed = StreamType(StreamKind::kSingleWebFeed);
  StreamType single_web_feed_a = StreamType(StreamKind::kSingleWebFeed, "A");
  StreamType unknown = StreamType();

  ASSERT_EQ(for_you.ToString(), "ForYou");
  ASSERT_EQ(following.ToString(), "WebFeed");
  ASSERT_EQ(single_web_feed.ToString(), "SingleWebFeed_");
  ASSERT_EQ(single_web_feed_a.ToString(), "SingleWebFeed_A");
  ASSERT_EQ(unknown.ToString(), "Unknown");
}
}  // namespace feed
