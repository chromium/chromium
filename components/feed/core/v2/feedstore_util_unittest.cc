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

std::deque<uint32_t> ContentHashRange(int start, int size) {
  std::deque<uint32_t> content_hashes;
  for (int i = start; i < start + size; ++i)
    content_hashes.push_back(i);
  return content_hashes;
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
  MaybeUpdateSessionId(metadata, absl::nullopt);
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
TEST(feedstore_util_test, StreamTypeFromKey) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType following = StreamType(StreamKind::kFollowing);
  StreamType channel = StreamType(StreamKind::kChannel);
  StreamType channel_a = StreamType(StreamKind::kChannel, "A");
  StreamType unknown = StreamType();

  EXPECT_EQ(StreamKey(for_you), kForYouStreamKey);
  EXPECT_EQ(StreamKey(following), kFollowStreamKey);
  EXPECT_DCHECK_DEATH(StreamKey(unknown));

  EXPECT_TRUE(StreamTypeFromKey(StreamKey(channel)).IsChannelFeed());
  EXPECT_TRUE(StreamTypeFromKey(StreamKey(channel_a)).IsChannelFeed());

  EXPECT_EQ(StreamTypeFromKey(StreamKey(channel_a)).GetWebFeedId(), "A");

  EXPECT_TRUE(StreamTypeFromKey(StreamKey(following)).IsWebFeed());
  EXPECT_TRUE(StreamTypeFromKey(StreamKey(for_you)).IsForYou());
  EXPECT_EQ(StreamTypeFromKey("z"), StreamType());
}

TEST(feedstore_util_test, AddMostRecentContentHashes_NotExceedingCap) {
  Metadata metadata;

  // Add an empty list.
  std::deque<uint32_t> content_hashes;
  AddMostRecentContentHashes(metadata, std::move(content_hashes));
  EXPECT_EQ(0, metadata.most_recent_content_hashes_size());

  // Add some contents.
  int offset = 0;
  int size = kMaxMostRecentContentHashes / 2;
  AddMostRecentContentHashes(metadata, ContentHashRange(offset, size));
  EXPECT_THAT(metadata.most_recent_content_hashes(),
              ElementsAreArray(ContentHashRange(offset, size)));

  // Add more contents. The combination of existing and new contents do not
  // exceed the max capacity of the most recent list.
  offset += size;
  size = kMaxMostRecentContentHashes / 4;
  AddMostRecentContentHashes(metadata, ContentHashRange(offset, size));
  EXPECT_THAT(metadata.most_recent_content_hashes(),
              ElementsAreArray(
                  ContentHashRange(0, kMaxMostRecentContentHashes / 2 + size)));
}

TEST(feedstore_util_test, AddMostRecentContentHashes_ExceedingCap) {
  Metadata metadata;

  // Add some content.
  int offset = 0;
  int size = kMaxMostRecentContentHashes * 0.75;
  AddMostRecentContentHashes(metadata, ContentHashRange(offset, size));
  EXPECT_THAT(metadata.most_recent_content_hashes(),
              ElementsAreArray(ContentHashRange(offset, size)));

  // Add more contents. The combination of existing and new contents exceed
  // the max capacity of the most recent list.
  offset += size;
  size = kMaxMostRecentContentHashes / 2;
  AddMostRecentContentHashes(metadata, ContentHashRange(offset, size));
  EXPECT_THAT(metadata.most_recent_content_hashes(),
              ElementsAreArray(
                  ContentHashRange(offset + size - kMaxMostRecentContentHashes,
                                   kMaxMostRecentContentHashes)));

  // Continue to add more contents.
  offset += size;
  size = kMaxMostRecentContentHashes / 3;
  AddMostRecentContentHashes(metadata, ContentHashRange(offset, size));
  EXPECT_THAT(metadata.most_recent_content_hashes(),
              ElementsAreArray(
                  ContentHashRange(offset + size - kMaxMostRecentContentHashes,
                                   kMaxMostRecentContentHashes)));

  // Continue to add a lot more contents which exceeds the max capacity of the
  // most recent list.
  offset += size;
  size = kMaxMostRecentContentHashes + 10;
  AddMostRecentContentHashes(metadata, ContentHashRange(offset, size));
  EXPECT_THAT(metadata.most_recent_content_hashes(),
              ElementsAreArray(ContentHashRange(
                  offset + size - kMaxMostRecentContentHashes - 10,
                  kMaxMostRecentContentHashes)));
}

}  // namespace
}  // namespace feedstore
