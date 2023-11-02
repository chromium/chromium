// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/inline_autocompletion_util.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(InlineAutocompletionUtilTest, FindAtWordbreak) {
  // Should find the first wordbreak occurrence.
  EXPECT_EQ(FindAtWordbreak(u"prefixmatch wordbreak_match", u"match"), 22u);

  // Should return npos when no occurrences exist.
  EXPECT_EQ(FindAtWordbreak(u"prefixmatch", u"match"), std::string::npos);

  // Should skip occurrences before |search_start|.
  EXPECT_EQ(FindAtWordbreak(u"match match", u"match", 1), 6u);
}

}  // namespace
