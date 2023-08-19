// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/disabled_sites.h"

#include "base/test/scoped_feature_list.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {
namespace {

TEST(DisabledSitesTest, AllPaths) {
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://www.youtube.com")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://www.youtube.com/somepage")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://m.youtube.com")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://m.youtube.com/somepage")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://youtube.com")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://youtube.com/somepage")));

#if BUILDFLAG(IS_IOS)
  // Don't rewrite this to just reference the kSharedHighlightingAmp feature -
  // the purpose of this test is to assert that the shared highlighting AMP
  // behavior is as expected per platform, so when we decide to enable shared
  // highlighting for iOS, set this constant to true. That way, if we
  // accidentally do enable it too early, this test will fail.
  constexpr bool kIsAmpEnabled = false;
#else
  constexpr bool kIsAmpEnabled = true;
#endif

  // Paths starting with /amp/ are disabled.
  EXPECT_EQ(kIsAmpEnabled,
            ShouldOfferLinkToText(GURL("https://www.google.com/amp/")));
  EXPECT_EQ(kIsAmpEnabled,
            ShouldOfferLinkToText(GURL("https://www.google.com/amp/foo")));
  EXPECT_EQ(kIsAmpEnabled,
            ShouldOfferLinkToText(GURL("https://google.com/amp/")));
  EXPECT_EQ(kIsAmpEnabled,
            ShouldOfferLinkToText(GURL("https://google.com/amp/foo")));

  // Other paths are not.
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.google.com")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.google.com/somepage")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://google.com")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://google.com/somepage")));

  // Paths with /amp/ later on are also not affected.
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://google.com/foo/amp/")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://google.com/foo/amp/bar")));
}

#if !BUILDFLAG(IS_IOS)
TEST(DisabledSitesTest, SpecificPagesAmpEnabled) {
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.google.com/amp/")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.google.com/amp/foo")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://google.com/amp/")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://google.com/amp/foo")));
}
#endif  // !IS_IOS

TEST(DisabledSitesTest, NonMatchingHost) {
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.example.com")));
}

TEST(DisabledSitesTest, SupportsLinkGenerationInIframe) {
  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://www.google.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(
      SupportsLinkGenerationInIframe(GURL("https://www.example.com/")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://mobile.google.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://mobile.foo.com")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://m.google.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://m.foo.com")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://www.bing.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://www.nyt.com")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://mobile.bing.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://mobile.nyt.com")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://m.bing.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://m.nyt.com")));

  EXPECT_FALSE(SupportsLinkGenerationInIframe(
      GURL("https://www.google.com/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(
      GURL("https://m.bing.com/a/www.nyt.com/ampthml/blogs.html")));
}

}  // namespace
}  // namespace shared_highlighting
