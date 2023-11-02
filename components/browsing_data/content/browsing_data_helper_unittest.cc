// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_helper.h"

#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace browsing_data {
namespace {

class BrowsingDataHelperTest : public testing::Test {
 public:
  BrowsingDataHelperTest() {}

  BrowsingDataHelperTest(const BrowsingDataHelperTest&) = delete;
  BrowsingDataHelperTest& operator=(const BrowsingDataHelperTest&) = delete;

  ~BrowsingDataHelperTest() override {}

  bool IsWebScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (HasWebScheme(test) && browsing_data::IsWebScheme(scheme));
  }
};

TEST_F(BrowsingDataHelperTest, WebStorageSchemesAreWebSchemes) {
  EXPECT_TRUE(IsWebScheme(url::kHttpScheme));
  EXPECT_TRUE(IsWebScheme(url::kHttpsScheme));
  EXPECT_TRUE(IsWebScheme(url::kFileScheme));
  EXPECT_TRUE(IsWebScheme(url::kFtpScheme));
  EXPECT_TRUE(IsWebScheme(url::kWsScheme));
  EXPECT_TRUE(IsWebScheme(url::kWssScheme));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreNotWebSchemes) {
  EXPECT_FALSE(IsWebScheme(url::kAboutScheme));
  EXPECT_FALSE(IsWebScheme(content::kChromeDevToolsScheme));
  EXPECT_FALSE(IsWebScheme(content::kChromeUIScheme));
  EXPECT_FALSE(IsWebScheme(url::kJavaScriptScheme));
  EXPECT_FALSE(IsWebScheme(url::kMailToScheme));
  EXPECT_FALSE(IsWebScheme(content::kViewSourceScheme));
}

TEST_F(BrowsingDataHelperTest, SchemesThatCantStoreDataDontMatchAnything) {
  EXPECT_FALSE(IsWebScheme(url::kDataScheme));
  EXPECT_FALSE(IsWebScheme("feed"));
  EXPECT_FALSE(IsWebScheme(url::kBlobScheme));
  EXPECT_FALSE(IsWebScheme(url::kFileSystemScheme));
  EXPECT_FALSE(IsWebScheme("invalid-scheme-i-just-made-up"));
}

}  // namespace
}  // namespace browsing_data
