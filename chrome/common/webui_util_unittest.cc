// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_util.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class WebUIUtilTest : public ::testing::Test,
                      public ::testing::WithParamInterface<bool> {
 public:
  WebUIUtilTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kWebUICodeCache,
        {{"RestrictedWebUICodeCache", GetParam() ? "true" : "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebUIUtilTest, ShouldUseCodeCacheForWebUIUrl) {
#if !BUILDFLAG(IS_ANDROID)
  // Only specifically targeted resources should participate if the restricted
  // code cache flag is enabled.
  EXPECT_TRUE(chrome::ShouldUseCodeCacheForWebUIUrl(
      GURL(chrome::kChromeUITabSearchURL)));
  EXPECT_EQ(!GetParam(), chrome::ShouldUseCodeCacheForWebUIUrl(
                             GURL(chrome::kChromeUIAboutURL)));
#else
  EXPECT_TRUE(
      chrome::ShouldUseCodeCacheForWebUIUrl(GURL(chrome::kChromeUIAboutURL)));
#endif
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebUIUtilTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<WebUIUtilTest::ParamType>& info) {
      return info.param ? "Restricted" : "Unrestricted";
    });
