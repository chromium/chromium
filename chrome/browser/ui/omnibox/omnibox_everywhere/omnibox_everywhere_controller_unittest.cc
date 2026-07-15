// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxEverywhereControllerTest : public testing::Test {
 public:
  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

 protected:
  // Must be initialized before `task_environment_` to ensure feature overrides
  // are registered before background threads are started.
  base::test::ScopedFeatureList feature_list_{omnibox::kOmniboxEverywhere};
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(OmniboxEverywhereControllerTest, EnabledFeatureInstantiatesController) {
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  GlobalFeatures* features = TestingBrowserProcess::GetGlobal()->GetFeatures();
  ASSERT_TRUE(features);
  EXPECT_TRUE(features->omnibox_everywhere_controller());
}
