// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"

// The helper class to verify the bubble app list with recent apps shown.
class BubbleAppListWithRecentAppBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  BubbleAppListWithRecentAppBrowserTest() = default;
  BubbleAppListWithRecentAppBrowserTest(
      const BubbleAppListWithRecentAppBrowserTest&) = delete;
  BubbleAppListWithRecentAppBrowserTest& operator=(
      const BubbleAppListWithRecentAppBrowserTest&) = delete;
  ~BubbleAppListWithRecentAppBrowserTest() override = default;

  // extensions::ExtensionBrowserTest:
  void SetUp() override {
    feature_list_.InitWithFeatures({ash::features::kProductivityLauncher},
                                   /*disabled_features=*/{});
    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();

    // Ensure async callbacks are run.
    base::RunLoop().RunUntilIdle();

    // Install enough apps to show the recent apps view.
    LoadExtension(test_data_dir_.AppendASCII("app1"));
    LoadExtension(test_data_dir_.AppendASCII("app2"));

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        browser()->window()->GetNativeWindow()->GetRootWindow());
    app_list_test_api_.ShowBubbleAppListAndWait();
  }

  ash::AppListTestApi app_list_test_api_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

IN_PROC_BROWSER_TEST_F(BubbleAppListWithRecentAppBrowserTest,
                       MouseClickAtRecentApp) {
  views::View* recent_app = app_list_test_api_.GetRecentAppAt(0);
  event_generator_->MoveMouseTo(recent_app->GetBoundsInScreen().CenterPoint());
  base::HistogramTester histogram_tester;
  event_generator_->ClickLeftButton();

  // Verify that the recent app activation is recorded.
  histogram_tester.ExpectBucketCount(
      "Apps.NewUserFirstLauncherAction.ClamshellMode",
      static_cast<int>(ash::AppListLaunchedFrom::kLaunchedFromRecentApps),
      /*expected_bucket_count=*/1);
}
