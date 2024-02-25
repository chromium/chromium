// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feedstore_util.h"

#include <string>
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;

namespace feedstore {
namespace {
base::Time kTestTimeEpoch = base::Time::UnixEpoch();
const base::Time kExpiryTime1 = kTestTimeEpoch + base::Hours(2);

const std::string Token1() {
  return "token1";
}
const std::string Token2() {
  return "token2";
}

TEST(feedstore_util_test, SetSessionId) {
  Metadata metadata;

  // Verify that directly calling SetSessionId works as expected.
  SetSessionId(metadata, Token1(), kExpiryTime1);

  EXPECT_EQ(Token1(), metadata.session_id().token());
  EXPECT_TIME_EQ(kExpiryTime1, GetSessionIdExpiryTime(metadata));
}

TEST(feedstore_util_test, MaybeUpdateSessionId) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  feedstore::Metadata metadata;
  SetSessionId(metadata, Token1(), kExpiryTime1);

  // Updating the token with nullopt is a NOP.
  MaybeUpdateSessionId(metadata, std::nullopt);
  EXPECT_EQ(Token1(), metadata.session_id().token());

  // Updating the token with the same value is a NOP.
  MaybeUpdateSessionId(metadata, Token1());
  EXPECT_EQ(Token1(), metadata.session_id().token());

  // Updating the token with a different value resets the token and assigns a
  // new expiry time.
  MaybeUpdateSessionId(metadata, Token2());
  EXPECT_EQ(Token2(), metadata.session_id().token());
  EXPECT_TIME_EQ(base::Time::Now() + feed::GetFeedConfig().session_id_max_age,
                 GetSessionIdExpiryTime(metadata));

  // Updating the token with the empty string clears its value.
  MaybeUpdateSessionId(metadata, "");
  EXPECT_TRUE(metadata.session_id().token().empty());
  EXPECT_TRUE(GetSessionIdExpiryTime(metadata).is_null());
}

TEST(feedstore_util_test, GetNextActionId) {
  Metadata metadata;

  EXPECT_EQ(feed::LocalActionId(1), GetNextActionId(metadata));
  EXPECT_EQ(feed::LocalActionId(2), GetNextActionId(metadata));
}

using feed::StreamKind;
using feed::StreamType;
TEST(feedstore_util_test, StreamTypeFromId) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType following = StreamType(StreamKind::kFollowing);
  StreamType single_web_feed = StreamType(StreamKind::kSingleWebFeed);
  StreamType single_web_feed_menu = StreamType(
      StreamKind::kSingleWebFeed, "A", feed::SingleWebFeedEntryPoint::kMenu);
  StreamType single_web_feed_other = StreamType(
      StreamKind::kSingleWebFeed, "A", feed::SingleWebFeedEntryPoint::kOther);
  StreamType supervised_feed = StreamType(StreamKind::kSupervisedUser);

  StreamType unknown = StreamType();

  EXPECT_EQ(StreamKey(for_you), kForYouStreamKey);
  EXPECT_EQ(StreamKey(following), kFollowStreamKey);
  EXPECT_EQ(StreamKey(supervised_feed), kSupervisedUserStreamKey);
  EXPECT_DCHECK_DEATH(StreamKey(unknown));

  EXPECT_TRUE(StreamTypeFromKey(StreamKey(single_web_feed)).IsSingleWebFeed());
  EXPECT_TRUE(
      StreamTypeFromKey(StreamKey(single_web_feed_menu)).IsSingleWebFeed());
  EXPECT_TRUE(
      StreamTypeFromKey(StreamKey(single_web_feed_other)).IsSingleWebFeed());

  EXPECT_EQ(StreamTypeFromKey(StreamKey(single_web_feed_menu)).GetWebFeedId(),
            "A");
  EXPECT_EQ(StreamTypeFromKey(StreamKey(single_web_feed_other)).GetWebFeedId(),
            "A");

  EXPECT_TRUE(StreamTypeFromKey(StreamKey(following)).IsWebFeed());
  EXPECT_TRUE(StreamTypeFromKey(StreamKey(for_you)).IsForYou());
  EXPECT_TRUE(
      StreamTypeFromKey(StreamKey(supervised_feed)).IsForSupervisedUser());
  EXPECT_EQ(StreamTypeFromKey("z"), StreamType());
}

}  // namespace
}  // namespace feedstore
