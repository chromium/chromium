// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/actor_util.h"

#include <cstddef>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

TEST(ActorUtilTest, TruncateValidUtf8ToByteSize_AsciiAndZero) {
  std::string empty;
  TruncateValidUtf8ToByteSize(empty, 0);
  EXPECT_EQ(empty, "");

  std::string s = "hello";
  TruncateValidUtf8ToByteSize(s, 10);
  EXPECT_EQ(s, "hello");

  TruncateValidUtf8ToByteSize(s, 5);
  EXPECT_EQ(s, "hello");

  TruncateValidUtf8ToByteSize(s, 3);
  EXPECT_EQ(s, "hel");

  TruncateValidUtf8ToByteSize(s, 0);
  EXPECT_EQ(s, "");
}

TEST(ActorUtilTest, TruncateValidUtf8ToByteSize_MultiByteBoundaries) {
  // 'a' (1 byte) + U+00E9 (2 bytes: C3 A9) + U+1F600 (4 bytes: F0 9F 98 80).
  const std::string input = "a\xC3\xA9\xF0\x9F\x98\x80";

  auto truncated = [&](size_t max_bytes) {
    std::string s = input;
    TruncateValidUtf8ToByteSize(s, max_bytes);
    return s;
  };

  EXPECT_EQ(truncated(0), "");
  EXPECT_EQ(truncated(1), "a");
  EXPECT_EQ(truncated(2), "a");
  EXPECT_EQ(truncated(3), "a\xC3\xA9");
  EXPECT_EQ(truncated(4), "a\xC3\xA9");
  EXPECT_EQ(truncated(5), "a\xC3\xA9");
  EXPECT_EQ(truncated(6), "a\xC3\xA9");
  EXPECT_EQ(truncated(7), input);
}

TEST(ActorUtilTest, TruncateValidUtf8ToByteSize_PreservesNoncharacters) {
  // U+FFFF (EF BF BF) repeated twice (6 bytes total).
  const std::string nonchars = "\xEF\xBF\xBF\xEF\xBF\xBF";

  std::string s = nonchars;
  TruncateValidUtf8ToByteSize(s, 5);
  EXPECT_EQ(s, "\xEF\xBF\xBF");

  s = nonchars;
  TruncateValidUtf8ToByteSize(s, 3);
  EXPECT_EQ(s, "\xEF\xBF\xBF");

  s = nonchars;
  TruncateValidUtf8ToByteSize(s, 2);
  EXPECT_EQ(s, "");

  s = nonchars;
  TruncateValidUtf8ToByteSize(s, 1);
  EXPECT_EQ(s, "");

  // 4-byte noncharacter U+10FFFF (F4 8F BF BF) preceded by ASCII.
  const std::string four_byte_nonchar = "x\xF4\x8F\xBF\xBF\xF4\x8F\xBF\xBF";
  s = four_byte_nonchar;
  TruncateValidUtf8ToByteSize(s, 8);
  EXPECT_EQ(s, "x\xF4\x8F\xBF\xBF");

  s = four_byte_nonchar;
  TruncateValidUtf8ToByteSize(s, 5);
  EXPECT_EQ(s, "x\xF4\x8F\xBF\xBF");

  s = four_byte_nonchar;
  TruncateValidUtf8ToByteSize(s, 4);
  EXPECT_EQ(s, "x");
}

}  // namespace
}  // namespace actor
