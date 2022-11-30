// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/config.h"

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/video_tutorials/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_tutorials {

TEST(VideoTutorialsConfigTest, FinchConfigEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> params = {
      {kBaseURLKey, "https://test.com"},
      {kPreferredLocaleConfigKey, "en"},
      {"fetch_frequency", "10"},
      {"experiment_tag", "{some_param:some_value}"}};
  feature_list.InitAndEnableFeatureWithParameters(features::kVideoTutorials,
                                                  params);

  EXPECT_EQ(Config::GetTutorialsServerURL("").spec(),
            "https://test.com/v1/videotutorials");
  EXPECT_EQ(Config::GetTutorialsServerURL("https://abc.com").spec(),
            "https://test.com/v1/videotutorials");
  EXPECT_EQ(Config::GetDefaultPreferredLocale(), "en");
  EXPECT_EQ(Config::GetFetchFrequency(), base::Days(10));
  EXPECT_EQ(Config::GetExperimentTag(), "{some_param:some_value}");
}

TEST(VideoTutorialsConfigTest, ConfigDefaultParams) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kVideoTutorials);
  EXPECT_EQ(Config::GetTutorialsServerURL("https://testing.com").spec(),
            "https://testing.com/v1/videotutorials");
  EXPECT_EQ(Config::GetTutorialsServerURL(""), GURL());
  EXPECT_EQ(Config::GetDefaultPreferredLocale(), "en");
  EXPECT_EQ(Config::GetFetchFrequency(), base::Days(15));
  EXPECT_EQ(Config::GetExperimentTag(), "");
}

}  // namespace video_tutorials
