// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intent_picker_tab_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/intent_helper/intent_chip_display_prefs.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS)

class IntentPickerTabHelperTest : public ChromeRenderViewHostTestHarness,
                                  public testing::WithParamInterface<
                                      apps::test::LinkCapturingFeatureVersion> {
 public:
  IntentPickerTabHelperTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam()), {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Associate the contents with the tab so that static lookups can find
    // the helper. In production this association is maintained by TabModel.
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(), &tab_);
    ON_CALL(tab_, GetUnownedUserDataHost())
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    helper_ = std::make_unique<IntentPickerTabHelper>(tab_, web_contents());
  }

  void TearDown() override {
    helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  IntentPickerTabHelper* helper() { return helper_.get(); }

  std::vector<apps::IntentPickerAppInfo> CreateTestAppList() {
    return {
        {apps::PickerEntryType::kWeb, ui::ImageModel(), "app_id", "Test app"}};
  }

 private:
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface tab_;
  std::unique_ptr<IntentPickerTabHelper> helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(IntentPickerTabHelperTest, ShowOrHideIcon) {
  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/true);

  ASSERT_TRUE(helper()->should_show_icon());

  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/false);

  ASSERT_FALSE(helper()->should_show_icon());
}

TEST_P(IntentPickerTabHelperTest, ShowIconForApps) {
  NavigateAndCommit(GURL("https://www.google.com"));
  helper()->MaybeShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->should_show_icon());
}

TEST_P(IntentPickerTabHelperTest, ShowIconForApps_ExpandedChip) {
  const GURL kTestUrl = GURL("https://www.google.com");

  NavigateAndCommit(kTestUrl);
  helper()->MaybeShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->ShouldShowExpandedChip());
}

TEST_P(IntentPickerTabHelperTest, ShowIconForApps_CollapsedChip) {
  const GURL kTestUrl = GURL("https://www.google.com");

  // Simulate having seen the chip for this URL several times before, so that it
  // appears collapsed.
  for (int i = 0; i < 3; i++) {
    IntentChipDisplayPrefs::GetChipStateAndIncrementCounter(profile(),
                                                            kTestUrl);
  }

  NavigateAndCommit(kTestUrl);
  helper()->MaybeShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->should_show_icon());
  ASSERT_FALSE(helper()->ShouldShowExpandedChip());
}

TEST_P(IntentPickerTabHelperTest, ShowIntentIcon_ResetsExpandedState) {
  const GURL kTestUrl = GURL("https://www.google.com");

  NavigateAndCommit(kTestUrl);
  helper()->MaybeShowIconForApps(CreateTestAppList());

  EXPECT_TRUE(helper()->should_show_icon());
  EXPECT_TRUE(helper()->ShouldShowExpandedChip());

  // Explicitly showing the icon should reset any app-based customizations.
  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/true);
  ASSERT_FALSE(helper()->ShouldShowExpandedChip());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(IntentPickerTabHelperTest, LinkCapturing_EntryPointShown) {
  base::HistogramTester histogram_tester;

  NavigateAndCommit(GURL("https://www.google.com"));

  // Create empty app list which ensures the intent picker icon is hidden.
  helper()->MaybeShowIconForApps({});

  histogram_tester.ExpectBucketCount("ChromeOS.Intents.IntentPickerIconEvent",
                                     apps::IntentPickerIconEvent::kIconShown,
                                     0);

  // None of the histograms should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 0);

  // Create app list with both a web and an ARC app, and show the intent picker
  // icon.
  {
    std::vector<apps::IntentPickerAppInfo> apps_list;
    apps_list = {
        {apps::PickerEntryType::kWeb, ui::ImageModel(), "app_id", "Test app"},
        {apps::PickerEntryType::kArc, ui::ImageModel(), "app_id", "Test app"}};
    helper()->MaybeShowIconForApps(std::move(apps_list));
  }

  histogram_tester.ExpectBucketCount("ChromeOS.Intents.IntentPickerIconEvent",
                                     apps::IntentPickerIconEvent::kIconShown,
                                     1);
  // All of the histograms should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 1);

  // Hide the intent picker icon.
  helper()->MaybeShowIconForApps({});

  // Create app list with only a web app and show the intent picker icon.
  {
    std::vector<apps::IntentPickerAppInfo> apps_list;
    apps_list = {
        {apps::PickerEntryType::kWeb, ui::ImageModel(), "app_id", "Test app"}};
    helper()->MaybeShowIconForApps(std::move(apps_list));
  }

  histogram_tester.ExpectBucketCount("ChromeOS.Intents.IntentPickerIconEvent",
                                     apps::IntentPickerIconEvent::kIconShown,
                                     2);
  // Only the web app and general histograms should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 2);

  // Hide the intent picker icon.
  helper()->MaybeShowIconForApps({});

  // Create app list with only an ARC app and show the intent picker icon.
  {
    std::vector<apps::IntentPickerAppInfo> apps_list;
    apps_list = {
        {apps::PickerEntryType::kArc, ui::ImageModel(), "app_id", "Test app"}};
    helper()->MaybeShowIconForApps(std::move(apps_list));
  }

  histogram_tester.ExpectBucketCount("ChromeOS.Intents.IntentPickerIconEvent",
                                     apps::IntentPickerIconEvent::kIconShown,
                                     3);
  // Only the ARC app and general histograms should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 3);

  // Hide the intent picker icon.
  helper()->MaybeShowIconForApps({});

  // Create app list with non-ARC and non-web types and show the intent picker
  // icon.
  {
    std::vector<apps::IntentPickerAppInfo> apps_list;
    apps_list = {{apps::PickerEntryType::kMacOs, ui::ImageModel(), "app_id",
                  "Test app"}};
    helper()->MaybeShowIconForApps(std::move(apps_list));
  }

  histogram_tester.ExpectBucketCount("ChromeOS.Intents.IntentPickerIconEvent",
                                     apps::IntentPickerIconEvent::kIconShown,
                                     4);
  // Only the general histogram should be incremented.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 4);
}
#else
TEST_P(IntentPickerTabHelperTest, IconShownMetricsTriggered) {
  base::HistogramTester histogram_tester;

  NavigateAndCommit(GURL("https://www.google.com"));

  // Create empty app list which ensures the intent picker icon is hidden.
  helper()->MaybeShowIconForApps({});
  histogram_tester.ExpectBucketCount(
      "Webapp.Site.Intents.IntentPickerIconEvent",
      apps::IntentPickerIconEvent::kIconShown, 0);
}
#endif  // #if BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerTabHelperTest,
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOn),
    apps::test::LinkCapturingVersionToString);
