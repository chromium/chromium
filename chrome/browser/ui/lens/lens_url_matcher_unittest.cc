// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_url_matcher.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace lens {

class LensUrlMatcherTest : public testing::Test {};

TEST_F(LensUrlMatcherTest, IsMatch) {
  std::string url_allow_filters = "[\"*\"]";
  std::string url_block_filters = "[\"a.com/login\",\"d.com\",\"e.edu\"]";
  std::string url_path_match_allow_filters = "[\"assignment\",\"homework\"]";
  std::string url_path_match_block_filters = "[\"tutor\"]";
  std::string url_path_forced_allowed_match_patterns = "[\"edu/.+\"]";
  std::string hashed_domain_block_filters_list =
      "900131403,3582115023,196958618";  // e.com, subdomain.f.com, g.com
  LensUrlMatcher matcher(
      url_allow_filters, url_block_filters, url_path_match_allow_filters,
      url_path_match_block_filters, url_path_forced_allowed_match_patterns,
      hashed_domain_block_filters_list);

  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.a.com/")));
  EXPECT_TRUE(matcher.IsMatch(GURL("https://www.a.com/assignment")));
  EXPECT_TRUE(matcher.IsMatch(GURL("https://www.a.com/homework")));
  // Match can be in any part of path.
  EXPECT_TRUE(matcher.IsMatch(GURL("https://www.b.com/1/anassignmentpage/2")));
  EXPECT_TRUE(matcher.IsMatch(GURL("https://www.c.com/your-homework-problem")));
  // Match is on path, not on domain.
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.homework.com/")));
  // Match is on path, not on query.
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.c.com/path?assignment=1")));
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.c.com/path?query=homework")));
  // Match is on path, not on fragment.
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.c.com/path#assignment1")));
  // Block patterns take precedence over allow patterns.
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.a.com/tutor/assignment")));
  // url_block_filters takes precedence over path matches.
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.a.com/login/assignments")));
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.d.com/homework")));

  // url_forced_allowed_match_patterns is blocked by the block url filters.
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.e.edu/tutor")));
  // url_forced_allowed_match_patterns is blocked by the block path
  // filters.
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.d.edu/tutor")));
  // url_forced_allowed_match_patterns skips the allowed path filters.
  EXPECT_TRUE(matcher.IsMatch(GURL("https://www.d.edu/something")));
  // URL not in url_forced_allowed_match_patterns and not in allowed path
  // filters.
  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.d.edu/")));
  // URL in url_forced_allowed_match_patterns and in allowed path filters.
  EXPECT_TRUE(matcher.IsMatch(GURL("https://www.d.edu/homework")));

  EXPECT_FALSE(matcher.IsMatch(GURL("https://www.e.com/homework")));
  EXPECT_TRUE(matcher.IsMatch(GURL("https://f.com/homework")));
  EXPECT_FALSE(matcher.IsMatch(GURL("https://subdomain.f.com/homework")));
  EXPECT_FALSE(matcher.IsMatch(GURL("https://.www.g.com./homework")));
}

}  // namespace lens
