// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace waap {
namespace {

TEST(IsForInitialWebUITest, FeaturesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kInitialWebUI, features::kInitialWebUIMetrics,
           features::kWebUIReloadButton});

  EXPECT_FALSE(
      IsForInitialWebUI(GURL(std::string(content::kChromeUIScheme) + "://" +
                             chrome::kChromeUIReloadButtonHost)));
}

TEST(IsForInitialWebUITest, FeaturesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInitialWebUI, features::kInitialWebUIMetrics,
       features::kWebUIReloadButton},
      {});

  EXPECT_TRUE(
      IsForInitialWebUI(GURL(std::string(content::kChromeUIScheme) + "://" +
                             chrome::kChromeUIReloadButtonHost)));
}

TEST(IsForInitialWebUITest, NonChromeScheme) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInitialWebUI, features::kInitialWebUIMetrics,
       features::kWebUIReloadButton},
      {});

  EXPECT_FALSE(IsForInitialWebUI(
      GURL(std::string("https") + "://" + chrome::kChromeUIReloadButtonHost)));
}

TEST(IsForInitialWebUITest, NonInitialWebUIHost) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInitialWebUI, features::kInitialWebUIMetrics,
       features::kWebUIReloadButton},
      {});

  EXPECT_FALSE(IsForInitialWebUI(
      GURL(std::string(content::kChromeUIScheme) + "://" + "wrong-host")));
}

TEST(IsInitialWebUIMetricsLoggingEnabledTest, FeaturesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kInitialWebUIMetrics});

  EXPECT_FALSE(IsInitialWebUIMetricsLoggingEnabled());
}

TEST(IsInitialWebUIMetricsLoggingEnabledTest, FeaturesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kInitialWebUIMetrics}, {});

  EXPECT_TRUE(IsInitialWebUIMetricsLoggingEnabled());
}

}  // namespace
}  // namespace waap
