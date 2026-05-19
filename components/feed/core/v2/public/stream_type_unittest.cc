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

TEST_F(StreamTypeTest, IdentityTest) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType unknown = StreamType();

  ASSERT_TRUE(for_you.IsForYou());
  ASSERT_TRUE(for_you.IsValid());

  ASSERT_FALSE(unknown.IsForYou());
  ASSERT_FALSE(unknown.IsValid());
}

TEST_F(StreamTypeTest, StringTest) {
  StreamType for_you = StreamType(StreamKind::kForYou);
  StreamType unknown = StreamType();

  ASSERT_EQ(for_you.ToString(), "ForYou");
  ASSERT_EQ(unknown.ToString(), "Unknown");
}
}  // namespace feed
