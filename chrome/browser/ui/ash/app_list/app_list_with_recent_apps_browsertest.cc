// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"

// The helper class to verify the bubble app list with recent apps shown.
class AppListWithRecentAppBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  AppListWithRecentAppBrowserTest() = default;
  AppListWithRecentAppBrowserTest(const AppListWithRecentAppBrowserTest&) =
      delete;
  AppListWithRecentAppBrowserTest& operator=(
      const AppListWithRecentAppBrowserTest&) = delete;
  ~AppListWithRecentAppBrowserTest() override = default;

  // extensions::ExtensionBrowserTest:
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

  void EnsureZeroStateSearchDone() {
    base::RunLoop run_loop;
    AppListClientImpl::GetInstance()
        ->search_controller()
        ->WaitForZeroStateCompletionForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  ash::AppListTestApi app_list_test_api_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

// TODO(crbug.com/335362001): Re-enable this test
IN_PROC_BROWSER_TEST_F(AppListWithRecentAppBrowserTest, MouseClickAtRecentApp) {
  views::View* recent_app = app_list_test_api_.GetRecentAppAt(0);
  ASSERT_TRUE(recent_app);
  event_generator_->MoveMouseTo(recent_app->GetBoundsInScreen().CenterPoint());
  base::HistogramTester histogram_tester;
  event_generator_->ClickLeftButton();

  // Verify that the recent app activation is recorded.
  histogram_tester.ExpectBucketCount(
      "Apps.NewUserFirstLauncherAction.ClamshellMode",
      static_cast<int>(ash::AppListLaunchedFrom::kLaunchedFromRecentApps),
      /*expected_bucket_count=*/1);
}

// Tests that recent apps are shown in tablet mode app list after transition
// from bubble launcher search to tablet mode.
IN_PROC_BROWSER_TEST_F(AppListWithRecentAppBrowserTest,
                       RecentAppsShownInTabletModeAfterClearingSearch) {
  // Minimize the browser window so tablet mode launcher becomes visible
  // immediately after transition to tablet mode.
  browser()->window()->Minimize();

  views::View* recent_app = app_list_test_api_.GetRecentAppAt(0);
  ASSERT_TRUE(recent_app);

  // Simulate launcher search.
  app_list_test_api_.SimulateSearch(u"foo");

  // Transition to tablet mode while bubble launcher is showing search UI, to
  // verify that recent apps are not empty if launcher is shown just after
  // clearing search.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window =*/false);
  EnsureZeroStateSearchDone();

  recent_app = app_list_test_api_.GetRecentAppAt(0);
  ASSERT_TRUE(recent_app);
}

// Tests that recent apps remaiin stable after exiting launcher search, even
// after uninstalling a shown recent apps (which forces recent apps view
// refresh).
IN_PROC_BROWSER_TEST_F(AppListWithRecentAppBrowserTest,
                       DISABLED_RecentAppsNotUpdatedAfterShowingSearch) {
  std::vector<std::string> initial_recent_apps =
      app_list_test_api_.GetRecentAppIds();
  ASSERT_EQ(4u, initial_recent_apps.size());

  // Install another app, and verify it shows up in recent apps once launcher is
  // reshown.
  const extensions::Extension* app_to_remove =
      LoadExtension(test_data_dir_.AppendASCII("app3"));
  ASSERT_TRUE(app_to_remove);

  std::vector<std::string> recent_apps_after_reshow = {
      app_to_remove->id(), initial_recent_apps[0], initial_recent_apps[1],
      initial_recent_apps[2], initial_recent_apps[3]};

  AppListClientImpl::GetInstance()->DismissView();
  app_list_test_api_.ShowBubbleAppListAndWait();
  EXPECT_EQ(recent_apps_after_reshow, app_list_test_api_.GetRecentAppIds());

  // Verify that newly installed apps do no pop in into recent apps while
  // launcher is shown.
  const extensions::Extension* most_recent_app =
      LoadExtension(test_data_dir_.AppendASCII("app4"));
  ASSERT_TRUE(most_recent_app);
  EXPECT_EQ(recent_apps_after_reshow, app_list_test_api_.GetRecentAppIds());

  // Go to search and back - verify there is still no pop-in in recent apps.
  app_list_test_api_.SimulateSearch(u"foo");
  event_generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  // Toggling search should not trigger zero state search, but if it does, make
  // sure the results are flushed, which will fail the test later on.
  EnsureZeroStateSearchDone();

  EXPECT_EQ(recent_apps_after_reshow, app_list_test_api_.GetRecentAppIds());

  // Uninstall an app shown in recent apps. Verify that the app is removed from
  // recent apps, but new app does not pop-in.
  UninstallExtension(app_to_remove->id());
  EXPECT_EQ(initial_recent_apps, app_list_test_api_.GetRecentAppIds());

  // Most recent apps should show up in recent apps after launcher is reshown.
  AppListClientImpl::GetInstance()->DismissView();
  app_list_test_api_.ShowBubbleAppListAndWait();

  recent_apps_after_reshow[0] = most_recent_app->id();
  EXPECT_EQ(recent_apps_after_reshow, app_list_test_api_.GetRecentAppIds());
}
