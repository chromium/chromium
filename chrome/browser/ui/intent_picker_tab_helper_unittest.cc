// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intent_picker_tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_prefs.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"

class IntentPickerTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    IntentPickerTabHelper::CreateForWebContents(web_contents());
    helper_ = IntentPickerTabHelper::FromWebContents(web_contents());
  }

  IntentPickerTabHelper* helper() { return helper_; }

  std::vector<apps::IntentPickerAppInfo> CreateTestAppList() {
    std::vector<apps::IntentPickerAppInfo> apps;
    apps.emplace_back(apps::PickerEntryType::kWeb, ui::ImageModel(), "app_id",
                      "Test app");
    return apps;
  }

 private:
  IntentPickerTabHelper* helper_;
};

TEST_F(IntentPickerTabHelperTest, ShowOrHideIcon) {
  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/true);

  ASSERT_TRUE(helper()->should_show_icon());

  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/false);

  ASSERT_FALSE(helper()->should_show_icon());
}

TEST_F(IntentPickerTabHelperTest, ShowIconForApps) {
  base::test::ScopedFeatureList feature_list(
      apps::features::kLinkCapturingUiUpdate);

  NavigateAndCommit(GURL("https://www.google.com"));
  helper()->ShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->should_show_icon());
}

TEST_F(IntentPickerTabHelperTest, ShowIconForApps_CollapsedChip) {
  base::test::ScopedFeatureList feature_list(
      apps::features::kLinkCapturingUiUpdate);
  const GURL kTestUrl = GURL("https://www.google.com");

  // Simulate having seen the chip for this URL several times before, so that it
  // appears collapsed.
  for (int i = 0; i < 3; i++) {
    IntentPickerAutoDisplayPrefs::GetChipStateAndIncrementCounter(profile(),
                                                                  kTestUrl);
  }

  NavigateAndCommit(kTestUrl);
  helper()->ShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->should_show_icon());
  ASSERT_TRUE(helper()->should_show_collapsed_chip());
}

TEST_F(IntentPickerTabHelperTest, ShowIntentIcon_ResetsCollapsedState) {
  base::test::ScopedFeatureList feature_list(
      apps::features::kLinkCapturingUiUpdate);
  const GURL kTestUrl = GURL("https://www.google.com");

  // Simulate having seen the chip for this URL several times before, so that it
  // appears collapsed.
  for (int i = 0; i < 3; i++) {
    IntentPickerAutoDisplayPrefs::GetChipStateAndIncrementCounter(profile(),
                                                                  kTestUrl);
  }

  NavigateAndCommit(kTestUrl);
  helper()->ShowIconForApps(CreateTestAppList());

  EXPECT_TRUE(helper()->should_show_icon());
  EXPECT_TRUE(helper()->should_show_collapsed_chip());

  // Explicitly showing the icon should reset any app-based customizations.
  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/true);
  ASSERT_FALSE(helper()->should_show_collapsed_chip());
}
