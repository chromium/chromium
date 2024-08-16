// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_util.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr char kResource1[] = "/resource1.j";
constexpr char kResource2[] = "/resource2.j";
constexpr char kResource3[] = "/resource3.j";
}  // namespace

class WebUIUtilTest : public ::testing::Test,
                      public ::testing::WithParamInterface<bool> {
 public:
  WebUIUtilTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kWebUICodeCache, {}},
           {features::kRestrictedWebUICodeCache,
            {{"RestrictedWebUICodeCacheResources",
              base::StrCat({kResource1, ",", kResource2})}}}},
          {});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kWebUICodeCache, {}}}, {features::kWebUICodeCache});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebUIUtilTest, ShouldUseCodeCacheForWebUIUrl) {
  // Only specifically targeted resources should participate if the restricted
  // code cache flag is enabled.
  EXPECT_TRUE(chrome::ShouldUseCodeCacheForWebUIUrl(
      content::GetWebUIURL(base::StrCat({"host", kResource1}))));
  EXPECT_TRUE(chrome::ShouldUseCodeCacheForWebUIUrl(
      content::GetWebUIURL(base::StrCat({"host", kResource2}))));
  EXPECT_EQ(!GetParam(),
            chrome::ShouldUseCodeCacheForWebUIUrl(
                content::GetWebUIURL(base::StrCat({"host", kResource3}))));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebUIUtilTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<WebUIUtilTest::ParamType>& info) {
      return info.param ? "Restricted" : "Unrestricted";
    });
