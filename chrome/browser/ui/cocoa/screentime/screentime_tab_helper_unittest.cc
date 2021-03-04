// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screentime {

using ScreentimeTabHelperTest = ::testing::Test;

TEST(ScreentimeTabHelperTest, NeverUsedInIncognito) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(TabHelper::kScreenTime);

  content::BrowserTaskEnvironment task_environment;

  auto profile = std::make_unique<TestingProfile>();
  auto* otr_profile =
      profile->GetOffTheRecordProfile(Profile::OTRProfileID::PrimaryID());

  EXPECT_TRUE(TabHelper::IsScreentimeEnabledForProfile(profile.get()));
  EXPECT_FALSE(TabHelper::IsScreentimeEnabledForProfile(otr_profile));
}

}  // namespace screentime
