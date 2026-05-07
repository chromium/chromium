// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_ui.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/multistep_filter/core/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter_internals {
namespace {

class MultistepFilterInternalsUITest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(MultistepFilterInternalsUITest, IsWebUIEnabled_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(multistep_filter::kMultistepFilter);

  MultistepFilterInternalsUIConfig config;
  EXPECT_TRUE(config.IsWebUIEnabled(&profile_));
}

TEST_F(MultistepFilterInternalsUITest, IsWebUIEnabled_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(multistep_filter::kMultistepFilter);

  MultistepFilterInternalsUIConfig config;
  EXPECT_FALSE(config.IsWebUIEnabled(&profile_));
}

TEST_F(MultistepFilterInternalsUITest, IsWebUIEnabled_OffTheRecord) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(multistep_filter::kMultistepFilter);

  Profile* incognito_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);

  MultistepFilterInternalsUIConfig config;
  EXPECT_FALSE(config.IsWebUIEnabled(incognito_profile));
}

}  // namespace
}  // namespace multistep_filter_internals
