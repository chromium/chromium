// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intent_picker_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_prefs.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS)

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
  raw_ptr<IntentPickerTabHelper> helper_;
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

TEST_F(IntentPickerTabHelperTest, ShowIconForApps_ExpandedChip) {
  base::test::ScopedFeatureList feature_list(
      apps::features::kLinkCapturingUiUpdate);
  const GURL kTestUrl = GURL("https://www.google.com");

  NavigateAndCommit(kTestUrl);
  helper()->ShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->ShouldShowExpandedChip());
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
  ASSERT_FALSE(helper()->ShouldShowExpandedChip());
}

TEST_F(IntentPickerTabHelperTest, ShowIntentIcon_ResetsExpandedState) {
  base::test::ScopedFeatureList feature_list(
      apps::features::kLinkCapturingUiUpdate);
  const GURL kTestUrl = GURL("https://www.google.com");

  NavigateAndCommit(kTestUrl);
  helper()->ShowIconForApps(CreateTestAppList());

  EXPECT_TRUE(helper()->should_show_icon());
  EXPECT_TRUE(helper()->ShouldShowExpandedChip());

  // Explicitly showing the icon should reset any app-based customizations.
  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/true);
  ASSERT_FALSE(helper()->ShouldShowExpandedChip());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(IntentPickerTabHelperTest, LinkCapturing_EntryPointShown) {
  base::HistogramTester histogram_tester;

  NavigateAndCommit(GURL("https://www.google.com"));

  // Create empty app list which ensures the intent picker icon is hidden.
  std::vector<apps::IntentPickerAppInfo> apps_list;
  helper()->ShowIconForApps(apps_list);

  // None of the histograms should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 0);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.ArcApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 0);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 0);

  // Create app list with both a web and an ARC app, and show the intent picker
  // icon.
  apps_list.emplace_back(apps::PickerEntryType::kWeb, ui::ImageModel(),
                         "app_id", "Test app");
  apps_list.emplace_back(apps::PickerEntryType::kArc, ui::ImageModel(),
                         "app_id", "Test app");
  helper()->ShowIconForApps(apps_list);

  // All of the histograms should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.ArcApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 1);

  // Hide the intent picker icon.
  apps_list.clear();
  helper()->ShowIconForApps(apps_list);

  // Create app list with only a web app and show the intent picker icon.
  apps_list.emplace_back(apps::PickerEntryType::kWeb, ui::ImageModel(),
                         "app_id", "Test app");
  helper()->ShowIconForApps(apps_list);

  // Only the web app and general histograms should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 2);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.ArcApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 2);

  // Hide the intent picker icon.
  apps_list.clear();
  helper()->ShowIconForApps(apps_list);

  // Create app list with only an ARC app and show the intent picker icon.
  apps_list.emplace_back(apps::PickerEntryType::kArc, ui::ImageModel(),
                         "app_id", "Test app");
  helper()->ShowIconForApps(apps_list);

  // Only the ARC app and general histograms should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 2);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.ArcApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 2);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 3);

  // Hide the intent picker icon.
  apps_list.clear();
  helper()->ShowIconForApps(apps_list);

  // Create app list with non-ARC and non-web types and show the intent picker
  // icon.
  apps_list.clear();
  apps_list.emplace_back(apps::PickerEntryType::kMacOs, ui::ImageModel(),
                         "app_id", "Test app");
  helper()->ShowIconForApps(apps_list);

  // Only the general histogram should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 2);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.ArcApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 2);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 4);
}
#endif  // #if BUILDFLAG(IS_CHROMEOS)
