// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/active_window_waiter.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class AppListSearchBrowserTest : public InProcessBrowserTest {
 protected:
  void SearchForSystemApp(aura::Window* primary_root_window,
                          const std::u16string app_query,
                          const std::string app_id) {
    // Ensure the System app is installed.
    Profile* profile = ProfileManager::GetActiveUserProfile();
    ASSERT_TRUE(profile);
    SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();

    // Associate `client` with the current profile.
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();

    // Show the launcher.
    client->ShowAppList(ash::AppListShowSource::kSearchKey);
    AppListTestApi().WaitForBubbleWindowInRootWindow(
        primary_root_window,
        /*wait_for_opening_animation=*/true);

    // The search box should be active.
    SearchBoxView* search_box_view = GetSearchBoxView();
    ASSERT_TRUE(search_box_view);
    EXPECT_TRUE(search_box_view->is_search_box_active());

    // Search for the app and wait for the result.
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
  }

  void ClickTopSearchResult(
      aura::Window* primary_root_window,
      const std::u16string app_title,
      SearchResultListView::SearchResultListType list_type) {
    SearchResultListView* top_result_list =
        AppListTestApi().GetTopVisibleSearchResultListView();
    ASSERT_TRUE(top_result_list);
    EXPECT_EQ(top_result_list->list_type_for_test(), list_type);

    SearchResultView* top_result_view = top_result_list->GetResultViewAt(0);
    ASSERT_TRUE(top_result_view);
    ASSERT_TRUE(top_result_view->result());

    EXPECT_EQ(app_title, top_result_view->result()->title());

    // Open the search result by clicking on it.
    ui::test::EventGenerator event_generator(primary_root_window);
    event_generator.MoveMouseTo(
        top_result_view->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
  }
};

class AppListSearchWithCustomizableShortcutsBrowserTest
    : public AppListSearchBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kSearchCustomizableShortcutsInLauncher};
};

IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest, SearchBuiltInApps) {
  const std::string app_id = web_app::kOsSettingsAppId;
  aura::Window* const primary_root_window = Shell::GetPrimaryRootWindow();

  SearchForSystemApp(primary_root_window, u"Settings", app_id);

  ActiveWindowWaiter window_waiter(primary_root_window);

  ClickTopSearchResult(primary_root_window, u"Settings",
                       SearchResultListView::SearchResultListType::kApps);

  // Wait for the OS Settings window to activate.
  aura::Window* app_window = window_waiter.Wait();
  ASSERT_TRUE(app_window);
  EXPECT_EQ(
      app_id,
      ShelfID::Deserialize(app_window->GetProperty(ash::kShelfIDKey)).app_id);
}

IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest, OpenFeedbackApp) {
  aura::Window* const primary_root_window = Shell::GetPrimaryRootWindow();
  SearchForSystemApp(primary_root_window, u"Feedback",
                     web_app::kOsFeedbackAppId);

  GURL feedback_url = GURL(kChromeUIOSFeedbackUrl);
  content::TestNavigationObserver navigation_observer(feedback_url);
  navigation_observer.StartWatchingNewWebContents();

  ClickTopSearchResult(primary_root_window, u"Feedback",
                       SearchResultListView::SearchResultListType::kApps);

  // Wait for the Feedback app to launch.
  navigation_observer.Wait();
  Browser* feedback_browser = FindSystemWebAppBrowser(
      browser()->profile(), SystemWebAppType::OS_FEEDBACK);
  EXPECT_TRUE(feedback_browser);
}

IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest, OpenShortcutsApp) {
  aura::Window* const primary_root_window = Shell::GetPrimaryRootWindow();
  SearchForSystemApp(primary_root_window, u"Key Shortcuts",
                     web_app::kShortcutCustomizationAppId);

  GURL shortcut_customization_url = GURL(kChromeUIShortcutCustomizationAppURL);
  content::TestNavigationObserver navigation_observer(
      shortcut_customization_url);
  navigation_observer.StartWatchingNewWebContents();

  ClickTopSearchResult(primary_root_window, u"Key Shortcuts",
                       SearchResultListView::SearchResultListType::kApps);

  // Wait for the Shortcut Customization app to launch.
  navigation_observer.Wait();
  Browser* shortcut_customization_browser = FindSystemWebAppBrowser(
      browser()->profile(), SystemWebAppType::SHORTCUT_CUSTOMIZATION);
  EXPECT_TRUE(shortcut_customization_browser);
}

// Flaky. See http://crbug.com/324930012.
IN_PROC_BROWSER_TEST_F(AppListSearchWithCustomizableShortcutsBrowserTest,
                       DISABLED_OpenShortcutsAppFromShortcut) {
  // Launch the app from the Launcher via searching for a shortcut
  aura::Window* const primary_root_window = Shell::GetPrimaryRootWindow();
  SearchForSystemApp(primary_root_window, u"Open notifications",
                     web_app::kShortcutCustomizationAppId);

  GURL shortcut_customization_url = GURL(kChromeUIShortcutCustomizationAppURL);
  content::TestNavigationObserver navigation_observer(
      shortcut_customization_url);
  navigation_observer.StartWatchingNewWebContents();

  ClickTopSearchResult(primary_root_window, u"Open notifications",
                       SearchResultListView::SearchResultListType::kHelp);

  // Wait for the Shortcut Customization app to launch.
  navigation_observer.Wait();
  Browser* shortcut_customization_browser = FindSystemWebAppBrowser(
      browser()->profile(), SystemWebAppType::SHORTCUT_CUSTOMIZATION);
  EXPECT_TRUE(shortcut_customization_browser);
}

}  // namespace
}  // namespace ash
