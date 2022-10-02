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
  StreamType channel_a = StreamType(StreamKind::kChannel, "A");
  StreamType channel_b = StreamType(StreamKind::kChannel, "B");
  StreamType channel = StreamType(StreamKind::kChannel);

  ASSERT_TRUE(for_you < following);
  ASSERT_TRUE(following < channel_a);
  ASSERT_TRUE(channel_a < channel_b);
  ASSERT_TRUE(for_you < channel_b);
  ASSERT_TRUE(channel_b == channel_b);
  ASSERT_FALSE(channel_a < channel);
  ASSERT_FALSE(channel < for_you);
  ASSERT_FALSE(for_you < for_you);
  ASSERT_FALSE(channel_b < channel_b);
}

TEST_F(StreamTypeTest, IdentityTest) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType following = StreamType(StreamKind::kFollowing);
  StreamType channel = StreamType(StreamKind::kChannel);
  StreamType unknown = StreamType();

  ASSERT_TRUE(for_you.IsForYou());
  ASSERT_FALSE(for_you.IsWebFeed());
  ASSERT_FALSE(for_you.IsChannelFeed());
  ASSERT_TRUE(for_you.IsValid());

  ASSERT_FALSE(following.IsForYou());
  ASSERT_TRUE(following.IsWebFeed());
  ASSERT_FALSE(following.IsChannelFeed());
  ASSERT_TRUE(following.IsValid());

  ASSERT_FALSE(channel.IsForYou());
  ASSERT_FALSE(channel.IsWebFeed());
  ASSERT_TRUE(channel.IsChannelFeed());
  ASSERT_TRUE(channel.IsValid());

  ASSERT_FALSE(unknown.IsForYou());
  ASSERT_FALSE(unknown.IsWebFeed());
  ASSERT_FALSE(unknown.IsChannelFeed());
  ASSERT_FALSE(unknown.IsValid());
}

TEST_F(StreamTypeTest, StringTest) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType following = StreamType(StreamKind::kFollowing);
  StreamType channel = StreamType(StreamKind::kChannel);
  StreamType channel_a = StreamType(StreamKind::kChannel, "A");
  StreamType unknown = StreamType();

  ASSERT_EQ(for_you.ToString(), "ForYou");
  ASSERT_EQ(following.ToString(), "WebFeed");
  ASSERT_EQ(channel.ToString(), "Channel_");
  ASSERT_EQ(channel_a.ToString(), "Channel_A");
  ASSERT_EQ(unknown.ToString(), "Unknown");
}
}  // namespace feed
