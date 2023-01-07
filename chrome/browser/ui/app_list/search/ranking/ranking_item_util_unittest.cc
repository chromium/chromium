// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ranking_item_util.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

TEST(RankingItemUtilTest, SimplifyUrlId) {
  // Test handling different kinds of scheme, domain, and path. These should all
  // be no-ops.
  EXPECT_EQ(SimplifyUrlId("scheme://domain.com/path"),
            "scheme://domain.com/path");
  EXPECT_EQ(SimplifyUrlId("://domain.com"), "://domain.com");
  EXPECT_EQ(SimplifyUrlId("domain.com/path"), "domain.com/path");
  EXPECT_EQ(SimplifyUrlId("domain.com:1123/path"), "domain.com:1123/path");
  EXPECT_EQ(SimplifyUrlId("://"), "://");

  // Test removing trailing slash.
  EXPECT_EQ(SimplifyUrlId("scheme://domain.com/"), "scheme://domain.com");
  EXPECT_EQ(SimplifyUrlId("scheme:///"), "scheme://");
  EXPECT_EQ(SimplifyUrlId("scheme://"), "scheme://");

  // Test removing queries and fragments.
  EXPECT_EQ(SimplifyUrlId("domain.com/path?query=query"), "domain.com/path");
  EXPECT_EQ(SimplifyUrlId("scheme://path?query=query#fragment"),
            "scheme://path");
  EXPECT_EQ(SimplifyUrlId("scheme://?query=query#fragment"), "scheme://");
}

TEST(RankingItemUtilTest, SimplifyGoogleDocsUrlId) {
  EXPECT_EQ(SimplifyGoogleDocsUrlId("docs.google.com/hash/edit?"),
            "docs.google.com/hash");
  EXPECT_EQ(SimplifyGoogleDocsUrlId(
                "http://docs.google.com/hash/view?query#fragment"),
            "http://docs.google.com/hash");
  EXPECT_EQ(SimplifyGoogleDocsUrlId("https://docs.google.com/d/document/hash/"),
            "https://docs.google.com/d/document/hash");

  // We only want to remove one /view or /edit from the end of the URL.
  EXPECT_EQ(SimplifyGoogleDocsUrlId("docs.google.com/edit/hash/view/view"),
            "docs.google.com/edit/hash/view");
}

TEST(RankingItemUtilTest, NormalizeAppID) {
  const std::string raw_id = "mgndgikekgjfcpckkfioiadnlibdjbkf";
  const std::string id_with_scheme =
      "chrome-extension://mgndgikekgjfcpckkfioiadnlibdjbkf";
  const std::string id_with_slash = "mgndgikekgjfcpckkfioiadnlibdjbkf/";
  const std::string id_with_scheme_and_slash =
      "chrome-extension://mgndgikekgjfcpckkfioiadnlibdjbkf/";

  EXPECT_EQ(NormalizeAppId(raw_id), raw_id);
  EXPECT_EQ(NormalizeAppId(id_with_scheme), raw_id);
  EXPECT_EQ(NormalizeAppId(id_with_slash), raw_id);
  EXPECT_EQ(NormalizeAppId(id_with_scheme_and_slash), raw_id);
}

}  // namespace app_list
