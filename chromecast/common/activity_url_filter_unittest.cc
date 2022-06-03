// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/activity_url_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromecast {

TEST(ActivityUrlFilterTest, TestWhitelistURLMatch) {
  ActivityUrlFilter filter(
      {"http://www.google.com/*", ".*://finance.google.com/"});
  EXPECT_TRUE(filter.UrlMatchesWhitelist(
      GURL("http://www.google.com/a_test_that_matches")));
  EXPECT_FALSE(filter.UrlMatchesWhitelist(
      GURL("http://www.goggles.com/i_should_not_match")));
  EXPECT_TRUE(
      filter.UrlMatchesWhitelist(GURL("http://finance.google.com/mystock")));
  EXPECT_TRUE(
      filter.UrlMatchesWhitelist(GURL("https://finance.google.com/mystock")));
  EXPECT_FALSE(filter.UrlMatchesWhitelist(GURL("https://www.google.com")));
  EXPECT_TRUE(filter.UrlMatchesWhitelist(GURL("http://www.google.com")));
}

}  // namespace chromecast
