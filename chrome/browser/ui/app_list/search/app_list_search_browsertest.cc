// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/help_app_ui/help_app_manager.h"
#include "chromeos/components/help_app_ui/help_app_manager_factory.h"
#include "chromeos/components/help_app_ui/search/search.mojom.h"
#include "chromeos/components/help_app_ui/search/search_handler.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

namespace app_list {

// This contains almost end-to-end tests for the launcher search backend. It is
// set up to simulate user input by calls to the AppListClient, and observe the
// results that would be displayed via the AppListModelUpdater. This class is
// also intended as in-code documentation for how to create future app list
// search integration tests.
class AppListSearchBrowserTest : public InProcessBrowserTest {
 public:
  using ResultType = ash::AppListSearchResultType;
  using DisplayType = ash::SearchResultDisplayType;

  AppListSearchBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kHelpAppLauncherSearch,
         chromeos::features::kHelpAppDiscoverTab},
        {});
  }
  ~AppListSearchBrowserTest() override = default;

  AppListSearchBrowserTest(const AppListSearchBrowserTest&) = delete;
  AppListSearchBrowserTest& operator=(const AppListSearchBrowserTest&) = delete;

  //---------------
  // Search helpers
  //---------------

  // The AppListClient is in charge of communication from ash to chrome, so can
  // be used to mimic UI actions. Examples include starting a search, launching
  // a result, or possibly activating a particular view.
  AppListClientImpl* GetClient() {
    auto* client = ::test::GetAppListClient();
    CHECK(client);
    return client;
  }

  void StartSearch(const std::string& query) {
    GetClient()->StartSearch(base::ASCIIToUTF16(query));
  }

  void SearchAndWaitForProviders(const std::string& query,
                                 const std::set<ResultType> providers) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    std::set<ResultType> finished_providers;
    const SearchController::ResultsChangedCallback callback =
        base::BindLambdaForTesting([&](ResultType provider) {
          finished_providers.insert(provider);

          // Quit the run loop if all |providers| are finished.
          for (const auto& type : providers) {
            if (finished_providers.find(type) == finished_providers.end())
              return;
          }
          quit_closure.Run();
        });

    // The ordering of this logic is important. The results changed callback
    // must be set before the call to StartSearch, to avoid a race between a
    // provider returning and the callback being set, which could lead to the
    // run loop timing out.
    GetClient()->search_controller()->set_results_changed_callback_for_test(
        std::move(callback));
    GetClient()->StartSearch(base::ASCIIToUTF16(query));
    run_loop.Run();
    // Once the run loop is finished, we have to remove the callback because the
    // referenced variables are about to go out of scope.
    GetClient()->search_controller()->set_results_changed_callback_for_test(
        base::DoNothing());
  }

  std::vector<ChromeSearchResult*> PublishedResults() {
    return GetClient()
        ->GetModelUpdaterForTest()
        ->GetPublishedSearchResultsForTest();
  }

  std::vector<ChromeSearchResult*> PublishedResultsForProvider(
      const ResultType provider) {
    std::vector<ChromeSearchResult*> results;
    for (auto* result : PublishedResults()) {
      if (result->result_type() == provider)
        results.push_back(result);
    }
    return results;
  }

  // Returns a search result for the given |id|, or nullptr if no matching
  // search result exists.
  ChromeSearchResult* FindResult(const std::string& id) {
    for (auto* result : PublishedResults()) {
      if (result->id() == id)
        return result;
    }
    return nullptr;
  }

  //----------------
  // Session helpers
  //----------------

  Profile* GetProfile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Simply tests that neither zero-state nor query-based search cause a crash.
IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest, SearchDoesntCrash) {
  // This won't catch everything, because not all providers run on all queries,
  // and so we can't wait for all providers to finish. Instead, we wait on one
  // app and one non-app provider.
  SearchAndWaitForProviders(
      "", {ResultType::kInstalledApp, ResultType::kZeroStateFile});
  SearchAndWaitForProviders(
      "some query", {ResultType::kInstalledApp, ResultType::kFileSearch});
}

// Test that clicking the Discover tab suggestion chip launches the Help app on
// the Discover page.
IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest,
                       ClickingDiscoverTabSuggestionChipLaunchesHelpApp) {
  web_app::WebAppProvider::Get(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);

  SearchAndWaitForProviders("", {ResultType::kHelpApp});

  ChromeSearchResult* result = FindResult("help-app://discover");
  ASSERT_TRUE(result);
  EXPECT_EQ(base::UTF16ToASCII(result->title()), "Make your own game");
  EXPECT_EQ(result->metrics_type(), ash::HELP_APP_DISCOVER);

  // Open the search result. This should open the help app at the expected url.
  size_t num_browsers = chrome::GetTotalBrowserCount();
  const GURL expected_url("chrome://help-app/discover");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  GetClient()->OpenSearchResult(
      GetClient()->GetModelUpdaterForTest()->model_id(), result->id(),
      ash::AppListSearchResultType::kHelpApp,
      /*event_flags=*/0, ash::AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      ash::AppListLaunchType::kAppSearchResult, /*suggestion_index=*/0,
      /*launch_as_default=*/false);

  navigation_observer.Wait();

  EXPECT_EQ(num_browsers + 1, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());

  // Clicking on the chip should stop showing it in the future.
  EXPECT_EQ(0, GetProfile()->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));
}

// Test that Help App shows up as Release notes if pref shows we have some times
// left to show it.
IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest,
                       AppListSearchHasReleaseNotesSuggestionChip) {
  web_app::WebAppProvider::Get(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  SearchAndWaitForProviders("", {ResultType::kHelpApp});

  auto* result = FindResult("help-app://updates");
  ASSERT_TRUE(result);
  EXPECT_EQ(base::UTF16ToASCII(result->title()), "What's new with Chrome OS");
  EXPECT_EQ(result->metrics_type(), ash::HELP_APP_UPDATES);
  // Displayed in first position.
  EXPECT_EQ(result->position_priority(), 1.0f);
  EXPECT_EQ(result->display_type(), DisplayType::kChip);
}

// Test that the number of times the suggestion chip should show decreases when
// the chip is shown.
IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest,
                       ReleaseNotesDecreasesTimesShownOnAppListOpen) {
  web_app::WebAppProvider::Get(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  // ShowAppList actually opens the app list and triggers |AppListShown| which
  // is where we decrease |kReleaseNotesSuggestionChipTimesLeftToShow|.
  GetClient()->ShowAppList();
  SearchAndWaitForProviders("", {ResultType::kHelpApp});

  const int times_left_to_show = GetProfile()->GetPrefs()->GetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow);
  EXPECT_EQ(times_left_to_show, 2);
}

// Test that clicking the Release Notes suggestion chip launches the Help app on
// the What's New page.
IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest,
                       ClickingReleaseNotesSuggestionChipLaunchesHelpApp) {
  web_app::WebAppProvider::Get(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  SearchAndWaitForProviders("", {ResultType::kHelpApp});

  ChromeSearchResult* result = FindResult("help-app://updates");

  // Open the search result. This should open the help app at the expected url.
  size_t num_browsers = chrome::GetTotalBrowserCount();
  const GURL expected_url("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  GetClient()->OpenSearchResult(
      GetClient()->GetModelUpdaterForTest()->model_id(), result->id(),
      ash::AppListSearchResultType::kHelpApp,
      /*event_flags=*/0, ash::AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      ash::AppListLaunchType::kAppSearchResult, /*suggestion_index=*/0,
      /*launch_as_default=*/false);

  navigation_observer.Wait();

  EXPECT_EQ(num_browsers + 1, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());

  // Clicking on the chip should stop showing it in the future.
  const int times_left_to_show = GetProfile()->GetPrefs()->GetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow);
  EXPECT_EQ(times_left_to_show, 0);
}

// Test that the help app provider provides list search results.
IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest,
                       HelpAppProviderProvidesListResults) {
  // Need this because it sets up the icon.
  web_app::WebAppProvider::Get(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  // Add some searchable content to the help app search handler.
  std::vector<chromeos::help_app::mojom::SearchConceptPtr> search_concepts;
  auto concept = chromeos::help_app::mojom::SearchConcept::New(
      /*id=*/"6318213",
      /*title=*/u"Fix connection problems",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"verycomplicatedsearchquery"},
      /*url_path_with_parameters=*/"help/id/test",
      /*locale=*/"");
  search_concepts.push_back(std::move(concept));

  base::RunLoop run_loop;
  chromeos::help_app::HelpAppManagerFactory::GetForBrowserContext(GetProfile())
      ->search_handler()
      ->Update(std::move(search_concepts), base::BindLambdaForTesting([&]() {
                 run_loop.QuitClosure().Run();
               }));
  // Wait until the update is complete.
  run_loop.Run();

  ChromeSearchResult* result = nullptr;
  while (!result) {
    // Search repeatedly until the desired result is found. Multiple searches
    // are needed because it takes time for the icon to load.
    SearchAndWaitForProviders("verycomplicatedsearchquery",
                              {ResultType::kHelpApp});

    // This gives a chance for the icon to load between searches.
    web_app::FlushSystemWebAppLaunchesForTesting(GetProfile());

    result = FindResult("chrome://help-app/help/id/test");
  }

  EXPECT_EQ(base::UTF16ToASCII(result->title()), "Fix connection problems");
  EXPECT_EQ(base::UTF16ToASCII(result->details()), "Help");
  // No priority for position.
  EXPECT_EQ(result->position_priority(), 0);
  EXPECT_EQ(result->display_type(), DisplayType::kList);

  // Open the search result. This should open the help app at the expected url
  // and log a metric indicating what content was launched.
  const size_t num_browsers = chrome::GetTotalBrowserCount();
  const GURL expected_url("chrome://help-app/help/id/test");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  base::HistogramTester histogram_tester;

  GetClient()->OpenSearchResult(
      GetClient()->GetModelUpdaterForTest()->model_id(), result->id(),
      ash::AppListSearchResultType::kHelpApp,
      /*event_flags=*/0, ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
      ash::AppListLaunchType::kAppSearchResult, /*suggestion_index=*/0,
      /*launch_as_default=*/false);
  navigation_observer.Wait();

  EXPECT_EQ(num_browsers + 1, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());
  // -20424143 is the hash of the content id. This hash value can be found in
  // the enum in the google-internal histogram file.
  histogram_tester.ExpectUniqueSample("Discover.LauncherSearch.ContentLaunched",
                                      -20424143, 1);
}

// Test that Help App shows up normally even when suggestion chip should show.
IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest, AppListSearchHasApp) {
  web_app::WebAppProvider::Get(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  SearchAndWaitForProviders(
      "", {ResultType::kInstalledApp, ResultType::kZeroStateFile,
           ResultType::kHelpApp});

  auto* result = FindResult(web_app::kHelpAppId);
  ASSERT_TRUE(result);
  // Has regular app name as title.
  EXPECT_EQ(base::UTF16ToASCII(result->title()), "Explore");
  // No priority for position.
  EXPECT_EQ(result->position_priority(), 0);
  // No override url (will open app at default page).
  EXPECT_FALSE(result->query_url().has_value());
  EXPECT_EQ(result->display_type(), DisplayType::kTile);
}

// This class contains additional logic to set up DriveFS and enable testing for
// Drive file search in the launcher.
class AppListDriveSearchBrowserTest : public AppListSearchBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ = base::BindRepeating(
        &AppListDriveSearchBrowserTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

 protected:
  virtual drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_path = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, std::string(), mount_path,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

 private:
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;
};

// Test that Drive files can be searched.
IN_PROC_BROWSER_TEST_F(AppListDriveSearchBrowserTest, DriveSearchTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  ASSERT_TRUE(drive_service->IsMounted());
  base::FilePath mount_path = drive_service->GetMountPointPath();

  ASSERT_TRUE(base::WriteFile(mount_path.Append("my_file.gdoc"), "content"));
  ASSERT_TRUE(
      base::WriteFile(mount_path.Append("other_file.gsheet"), "content"));

  SearchAndWaitForProviders("my", {ResultType::kDriveSearch});

  const auto results = PublishedResultsForProvider(ResultType::kDriveSearch);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0]);
  EXPECT_EQ(base::UTF16ToASCII(results[0]->title()), "my_file");
}

// Test that Drive folders can be searched.
IN_PROC_BROWSER_TEST_F(AppListDriveSearchBrowserTest, DriveFolderTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  ASSERT_TRUE(drive_service->IsMounted());
  base::FilePath mount_path = drive_service->GetMountPointPath();

  ASSERT_TRUE(base::CreateDirectory(mount_path.Append("my_folder")));
  ASSERT_TRUE(base::CreateDirectory(mount_path.Append("other_folder")));

  SearchAndWaitForProviders("my", {ResultType::kDriveSearch});

  const auto results = PublishedResultsForProvider(ResultType::kDriveSearch);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0]);
  EXPECT_EQ(base::UTF16ToASCII(results[0]->title()), "my_folder");
}

}  // namespace app_list
