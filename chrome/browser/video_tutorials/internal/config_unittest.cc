// Copyright 2020 The Chromium Authors. All rights reserved.
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
      {kBaseURLKey, "https://test.com"}, {kPreferredLocaleConfigKey, "en"}};
  feature_list.InitAndEnableFeatureWithParameters(features::kVideoTutorials,
                                                  params);

  EXPECT_EQ(Config::GetTutorialsServerURL().spec(),
            "https://test.com/v1/videotutorials");
  EXPECT_EQ(Config::GetDefaultPreferredLocale(), "en");
}

TEST(VideoTutorialsConfigTest, ConfigDefaultParams) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kVideoTutorials);
  EXPECT_EQ(Config::GetTutorialsServerURL().spec(),
            "https://chromeupboarding-pa.googleapis.com/v1/videotutorials");
  EXPECT_EQ(Config::GetDefaultPreferredLocale(), "hi");
}

}  // namespace video_tutorials
