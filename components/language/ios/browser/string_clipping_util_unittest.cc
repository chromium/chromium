// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/ios/browser/string_clipping_util.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {
namespace {

// Tests that a regular sentence is clipped correctly.
TEST(StringByClippingLastWordTest, ClipRegularSentence) {
  const std::u16string kInput = u"\nSome text here and there.";
  EXPECT_EQ(kInput, GetStringByClippingLastWord(kInput, 100));
}

// Tests that a long sentence exceeding some length is clipped correctly.
TEST(StringByClippingLastWordTest, ClipLongSentence) {
  // An arbitrary length.
  const size_t kStringLength = 10;
  std::u16string string(kStringLength, 'a');
  string.append(u" b cdefghijklmnopqrstuvwxyz");
  // The string should be cut at the last whitespace, after the 'b' character.
  std::u16string result =
      GetStringByClippingLastWord(string, kStringLength + 3);
  EXPECT_EQ(kStringLength + 2, result.size());
  EXPECT_EQ(0u, string.find_first_of(result));
}

// Tests that a block of text with no space is truncated to kLongStringLength.
TEST(StringByClippingLastWordTest, ClipLongTextContentNoSpace) {
  // Very long string.
  const size_t kLongStringLength = 65536;
  // A string slightly longer than |kLongStringLength|.
  std::u16string long_string(kLongStringLength + 10, 'a');
  // Block of text with no space should be truncated to kLongStringLength.
  std::u16string result =
      GetStringByClippingLastWord(long_string, kLongStringLength);
  EXPECT_EQ(kLongStringLength, result.size());
  EXPECT_EQ(0u, long_string.find_first_of(result));
}

}  // namespace
}  // namespace language
