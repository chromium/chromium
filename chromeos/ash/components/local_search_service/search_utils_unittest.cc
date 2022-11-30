// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/search_utils.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

TEST(SearchUtilsTest, PrefixMatch) {
  // Query is a prefix of text, score is the ratio.
  EXPECT_NEAR(ExactPrefixMatchScore(u"musi", u"music"), 0.8, 0.001);

  // Text is a prefix of query, score is 0.
  EXPECT_EQ(ExactPrefixMatchScore(u"music", u"musi"), 0);

  // Case matters.
  EXPECT_EQ(ExactPrefixMatchScore(u"musi", u"Music"), 0);

  // Query isn't a prefix.
  EXPECT_EQ(ExactPrefixMatchScore(u"wide", u"wifi"), 0);

  // Text is empty.
  EXPECT_EQ(ExactPrefixMatchScore(u"music", u""), 0);

  // Query is empty.
  EXPECT_EQ(ExactPrefixMatchScore(u"", u"abc"), 0);
}

TEST(SearchUtilsTest, BlockMatch) {
  EXPECT_NEAR(BlockMatchScore(u"wifi", u"wi-fi"), 0.804, 0.001);

  // Case matters.
  EXPECT_NEAR(BlockMatchScore(u"wifi", u"Wi-Fi"), 0.402, 0.001);
}

TEST(SearchUtilsTest, RelevanceCoefficient) {
  // Relevant because prefix requirement is met.
  EXPECT_GT(RelevanceCoefficient(u"wifi", u"wi-fi", 0 /* prefix_threshold */,
                                 0.99 /* block_threshold */),
            0);

  // Relevant because prefix requirement is met.
  EXPECT_GT(RelevanceCoefficient(u"musi", u"music", 0.8 /* prefix_threshold */,
                                 0.999 /* block_threshold */),
            0);

  // Relevant because block match requirement is met.
  EXPECT_GT(RelevanceCoefficient(u"wifi", u"wi-fi", 0.2 /* prefix_threshold */,
                                 0.001 /* block_threshold */),
            0);

  // Neither prefix nor block match requirement is met.
  EXPECT_EQ(RelevanceCoefficient(u"wifi", u"wi-fi", 0.2 /* prefix_threshold */,
                                 0.99 /* block_threshold */),
            0);

  // Return the higher score if both requirements are met.
  EXPECT_NEAR(RelevanceCoefficient(u"wifi", u"wi-fi", 0 /* prefix_threshold */,
                                   0 /* block_threshold */),
              std::max(BlockMatchScore(u"wifi", u"wi-fi"),
                       ExactPrefixMatchScore(u"wifi", u"wi-fi")),
              0.001);
  EXPECT_NEAR(RelevanceCoefficient(u"musi", u"music", 0 /* prefix_threshold */,
                                   0 /* block_threshold */),
              std::max(BlockMatchScore(u"musi", u"music"),
                       ExactPrefixMatchScore(u"musi", u"music")),
              0.001);
}

}  // namespace ash::local_search_service
