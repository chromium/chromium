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
#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Waits for a window to be activated and returns it via the Wait() method.
class ActiveWindowWaiter : public wm::ActivationChangeObserver {
 public:
  explicit ActiveWindowWaiter(aura::Window* root_window)
      : root_window_(root_window) {
    wm::GetActivationClient(root_window_)->AddObserver(this);
  }

  ~ActiveWindowWaiter() override {
    if (!found_window_) {
      wm::GetActivationClient(root_window_)->RemoveObserver(this);
    }
  }

  aura::Window* Wait() {
    run_loop_.Run();
    return found_window_;
  }

  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (gained_active) {
      found_window_ = gained_active;
      wm::GetActivationClient(root_window_)->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<aura::Window> root_window_ = nullptr;
  raw_ptr<aura::Window> found_window_ = nullptr;
};

class AppListSearchBrowserTest : public InProcessBrowserTest {
 public:
  AppListSearchBrowserTest() {
    // No need for a browser window.
    set_launch_browser_for_testing(nullptr);
  }
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

  // The search result should exist in the view hierarchy.
  AppListTestHelper helper;
  AppListSearchView* search_view = helper.GetBubbleAppListSearchView();
  std::vector<SearchResultContainerView*> result_containers =
      search_view->result_container_views_for_test();
  // The result is of type "App", in container index 2.
  ASSERT_GE(result_containers.size(), 2u);
  SearchResultContainerView* container = result_containers[2];
  SearchResultListView* list_view =
      static_cast<SearchResultListView*>(container);
  ASSERT_EQ(list_view->list_type_for_test(),
            SearchResultListView::SearchResultListType::kApps);

  // The result is the first entry in the container.
  SearchResultView* result_view = list_view->GetResultViewAt(0);
  EXPECT_TRUE(result_view);

  // Open the search result. In tests, the result view doesn't have a "result"
  // associated with it so the test cannot directly activate the view. Activate
  // at the client level instead.
  ActiveWindowWaiter window_waiter(primary_root_window);
  AppListModelUpdater* model_updater = ::test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);
  client->OpenSearchResult(model_updater->model_id(), app_id, ui::EF_NONE,
                           AppListLaunchedFrom::kLaunchedFromSearchBox,
                           AppListLaunchType::kAppSearchResult, 0,
                           /*launch_as_default=*/false);

  // Wait for the OS Settings window to activate.
  aura::Window* app_window = window_waiter.Wait();
  ASSERT_TRUE(app_window);
  EXPECT_EQ(
      app_id,
      ShelfID::Deserialize(app_window->GetProperty(ash::kShelfIDKey)).app_id);
}

}  // namespace
}  // namespace ash
