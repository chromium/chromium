// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/active_window_waiter.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class AppListSearchBrowserTest : public InProcessBrowserTest {
 public:
  AppListSearchBrowserTest() {
    // No need for a browser window.
    set_launch_browser_for_testing(nullptr);
  }
};

class AppListSearchWithAppShortcutsBrowserTest
    : public AppListSearchBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kCrosWebAppShortcutUiUpdate};
};

IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest, SearchBuiltInApps) {
  // Ensure the OS Settings app is installed.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();

  // Associate `client` with the current profile.
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();

  // Show the launcher.
  aura::Window* const primary_root_window = Shell::GetPrimaryRootWindow();
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  AppListTestApi().WaitForBubbleWindowInRootWindow(
      primary_root_window,
      /*wait_for_opening_animation=*/true);

  // The search box should be active.
  SearchBoxView* search_box_view = GetSearchBoxView();
  ASSERT_TRUE(search_box_view);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Search for OS Settings and wait for the result.
  const std::u16string app_query = u"Settings";
  const std::string app_id = web_app::kOsSettingsAppId;
  app_list::SearchResultsChangedWaiter results_changed_waiter(
      AppListClientImpl::GetInstance()->search_controller(),
      {app_list::ResultType::kInstalledApp});
  app_list::ResultsWaiter results_waiter(app_query);

  AppListTestApi().SimulateSearch(app_query);

  results_changed_waiter.Wait();
  results_waiter.Wait();

  // Search UI updates are scheduled by posting a task on the main thread, run
  // loop to run scheduled result update tasks.
  base::RunLoop().RunUntilIdle();

  SearchResultListView* top_result_list =
      AppListTestApi().GetTopVisibleSearchResultListView();
  ASSERT_TRUE(top_result_list);
  EXPECT_EQ(top_result_list->list_type_for_test(),
            SearchResultListView::SearchResultListType::kApps);
  SearchResultView* top_result_view = top_result_list->GetResultViewAt(0);
  ASSERT_TRUE(top_result_view);
  ASSERT_TRUE(top_result_view->result());

  EXPECT_EQ(u"Settings", top_result_view->result()->title());

  ActiveWindowWaiter window_waiter(primary_root_window);

  // Open the search result by clicking on it.
  ui::test::EventGenerator event_generator(primary_root_window);
  event_generator.MoveMouseTo(
      top_result_view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  // Wait for the OS Settings window to activate.
  aura::Window* app_window = window_waiter.Wait();
  ASSERT_TRUE(app_window);
  EXPECT_EQ(
      app_id,
      ShelfID::Deserialize(app_window->GetProperty(ash::kShelfIDKey)).app_id);
}

IN_PROC_BROWSER_TEST_F(AppListSearchWithAppShortcutsBrowserTest,
                       SearchWebAppShortcut) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  // Associate `client` with the current profile.
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();

  // Show the launcher.
  aura::Window* const primary_root_window = Shell::GetPrimaryRootWindow();
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  AppListTestApi().WaitForBubbleWindowInRootWindow(
      primary_root_window,
      /*wait_for_opening_animation=*/true);

  // The search box should be active.
  SearchBoxView* search_box_view = GetSearchBoxView();
  ASSERT_TRUE(search_box_view);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Install a web based app shortcut.
  GURL shortcut_url = GURL("http://example.org/");
  std::u16string shortcut_name = u"Example";
  web_app::test::InstallShortcut(
      profile, base::UTF16ToUTF8(shortcut_name), shortcut_url,
      /*create_default_icon =*/true, /*is_policy_install=*/false);

  // Search for the shortcut and wait for the result.
  const std::u16string app_query = u"Example";
  app_list::SearchResultsChangedWaiter results_changed_waiter(
      AppListClientImpl::GetInstance()->search_controller(),
      {app_list::ResultType::kAppShortcutV2});
  app_list::ResultsWaiter results_waiter(app_query);

  AppListTestApi().SimulateSearch(app_query);

  results_changed_waiter.Wait();
  results_waiter.Wait();

  // Search UI updates are scheduled by posting a task on the main thread, run
  // loop to run scheduled result update tasks.
  base::RunLoop().RunUntilIdle();

  SearchResultListView* top_result_list =
      AppListTestApi().GetTopVisibleSearchResultListView();
  ASSERT_TRUE(top_result_list);
  EXPECT_EQ(top_result_list->list_type_for_test(),
            SearchResultListView::SearchResultListType::kAppShortcuts);
  SearchResultView* top_result_view = top_result_list->GetResultViewAt(0);
  ASSERT_TRUE(top_result_view);
  ASSERT_TRUE(top_result_view->result());

  EXPECT_EQ(u"Example", top_result_view->result()->title());

  ActiveWindowWaiter window_waiter(primary_root_window);

  // Open the search result by clicking on it.
  ui::test::EventGenerator event_generator(primary_root_window);
  event_generator.MoveMouseTo(
      top_result_view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  // Wait for the app shortcut window to activate.
  aura::Window* app_window = window_waiter.Wait();
  ASSERT_TRUE(app_window);
  EXPECT_EQ(
      app_constants::kChromeAppId,
      ShelfID::Deserialize(app_window->GetProperty(ash::kShelfIDKey)).app_id);
}

}  // namespace
}  // namespace ash
