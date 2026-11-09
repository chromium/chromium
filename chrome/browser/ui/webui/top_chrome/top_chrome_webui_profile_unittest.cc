// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webui {
namespace {

class TopChromeWebUIProfileTest : public testing::Test {
 protected:
  TopChromeWebUIProfileTest() {
    feature_list_.InitAndEnableFeature(features::kTopChromeWebUISharedRenderer);
  }

  void TearDown() override {
    content::RenderProcessHost::SetRunRendererInProcess(false);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile user_profile_;
};

TEST_F(TopChromeWebUIProfileTest, UsesTopChromeWebUIProfile) {
  ScopedTopChromeWebUIProfileForTesting scoped_profile;
  EXPECT_TRUE(ShouldUseTopChromeWebUIProfile());
  EXPECT_EQ(GetTopChromeWebUIProfile(),
            &GetProfileForTopChromeWebUIContents(&user_profile_));
}

// With --single-process, the only renderer process can't host the WebUIs of
// another BrowserContext, so the WebUIs stay in the user profile.
TEST_F(TopChromeWebUIProfileTest, UsesUserProfileInSingleProcess) {
  content::RenderProcessHost::SetRunRendererInProcess(true);
  EXPECT_FALSE(ShouldUseTopChromeWebUIProfile());
  EXPECT_EQ(&user_profile_,
            &GetProfileForTopChromeWebUIContents(&user_profile_));
}

}  // namespace
}  // namespace webui
