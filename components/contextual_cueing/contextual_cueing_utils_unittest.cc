// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_cueing/contextual_cueing_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_cueing {

TEST(ContextualCueingUtilsTest, IsHomepageUrl) {
  // Empty or invalid URLs.
  EXPECT_FALSE(IsHomepageUrl(GURL()));
  EXPECT_FALSE(IsHomepageUrl(GURL("invalid")));

  // Root URLs and variations.
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/index.html")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/index.php")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/default.aspx")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/home.htm")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/homepage")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/main.html")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/welcome")));

  // Locale prefixed homepages.
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/en")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/en/")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/en/index.html")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/us/en")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/us/en/")));
  EXPECT_TRUE(IsHomepageUrl(GURL("https://example.com/us/en/home.htm")));

  // Non-homepage URLs.
  EXPECT_FALSE(IsHomepageUrl(GURL("https://example.com/products/item1")));
  EXPECT_FALSE(IsHomepageUrl(GURL("https://example.com/article/news")));
  EXPECT_FALSE(IsHomepageUrl(GURL("https://example.com/user/profile")));
  EXPECT_FALSE(IsHomepageUrl(GURL("https://example.com/search?q=test")));
  EXPECT_FALSE(IsHomepageUrl(GURL("https://example.com/docs/intro.html")));
}

}  // namespace contextual_cueing
