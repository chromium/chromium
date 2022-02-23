// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/app_list/search/app_list_search_test_helper.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/with_crosapi_param.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"

using web_app::test::CrosapiParam;
using web_app::test::WithCrosapiParam;

namespace app_list {

class HelpAppSearchBrowserTest : public AppListSearchBrowserTest {
 public:
  HelpAppSearchBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kHelpAppLauncherSearch,
         chromeos::features::kHelpAppDiscoverTab},
        {});
  }
  ~HelpAppSearchBrowserTest() override = default;

  HelpAppSearchBrowserTest(const HelpAppSearchBrowserTest&) = delete;
  HelpAppSearchBrowserTest& operator=(const HelpAppSearchBrowserTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that clicking the Discover tab suggestion chip launches the Help app on
// the Discover page.
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       ClickingDiscoverTabSuggestionChipLaunchesHelpApp) {
  web_app::WebAppProvider::GetForTest(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);

  SearchAndWaitForProviders("", {ResultType::kHelpApp});

  ChromeSearchResult* result = FindResult("help-app://discover");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->title(), l10n_util::GetStringUTF16(
                                 IDS_HELP_APP_DISCOVER_TAB_SUGGESTION_CHIP));
  EXPECT_EQ(result->metrics_type(), ash::HELP_APP_DISCOVER);

  // Open the search result. This should open the help app at the expected url.
  size_t num_browsers = chrome::GetTotalBrowserCount();
  const GURL expected_url("chrome://help-app/discover");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  GetClient()->OpenSearchResult(
      GetClient()->GetModelUpdaterForTest()->model_id(), result->id(),
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
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       AppListSearchHasReleaseNotesSuggestionChip) {
  web_app::WebAppProvider::GetForTest(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  SearchAndWaitForProviders("", {ResultType::kHelpApp});

  auto* result = FindResult("help-app://updates");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->title(),
            l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_SUGGESTION_CHIP));
  EXPECT_EQ(result->metrics_type(), ash::HELP_APP_UPDATES);
  // Displayed in first position.
  EXPECT_EQ(result->position_priority(), 1.0f);
  EXPECT_EQ(result->display_type(), DisplayType::kChip);
}

// Test that the number of times the suggestion chip should show decreases when
// the chip is shown.
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       ReleaseNotesDecreasesTimesShownOnAppListOpen) {
  if (ash::features::IsProductivityLauncherEnabled())
    return;

  web_app::WebAppProvider::GetForTest(GetProfile())
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
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       ClickingReleaseNotesSuggestionChipLaunchesHelpApp) {
  web_app::WebAppProvider::GetForTest(GetProfile())
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
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       HelpAppProviderProvidesListResults) {
  // Need this because it sets up the icon.
  web_app::WebAppProvider::GetForTest(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  // Add some searchable content to the help app search handler.
  std::vector<ash::help_app::mojom::SearchConceptPtr> search_concepts;
  auto concept = ash::help_app::mojom::SearchConcept::New(
      /*id=*/"6318213",
      /*title=*/u"Fix connection problems",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"verycomplicatedsearchquery"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help/id/test",
      /*locale=*/"");
  search_concepts.push_back(std::move(concept));

  base::RunLoop run_loop;
  ash::help_app::HelpAppManagerFactory::GetForBrowserContext(GetProfile())
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

class HelpAppSwaSearchBrowserTest : public HelpAppSearchBrowserTest,
                                    public WithCrosapiParam {};

// Test that Help App shows up normally even when suggestion chip should show.
IN_PROC_BROWSER_TEST_P(HelpAppSwaSearchBrowserTest, AppListSearchHasApp) {
  web_app::WebAppProvider::GetForTest(GetProfile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  SearchAndWaitForProviders("",
                            {ResultType::kInstalledApp, ResultType::kHelpApp});

  auto* result = FindResult(web_app::kHelpAppId);
  ASSERT_TRUE(result);
  // Has regular app name as title.
  EXPECT_EQ(base::UTF16ToASCII(result->title()), "Explore");
  // No priority for position.
  EXPECT_EQ(result->position_priority(), 0);
  // No override url (will open app at default page).
  EXPECT_FALSE(result->query_url().has_value());
}

IN_PROC_BROWSER_TEST_P(HelpAppSwaSearchBrowserTest, Launch) {
  Profile* profile = browser()->profile();
  auto& system_web_app_manager =
      web_app::WebAppProvider::GetForTest(profile)->system_web_app_manager();
  system_web_app_manager.InstallSystemAppsForTesting();
  const web_app::AppId app_id = web_app::kHelpAppId;

  SearchAndWaitForProviders("",
                            {ResultType::kInstalledApp, ResultType::kHelpApp});
  auto* result = FindResult(web_app::kHelpAppId);
  ASSERT_TRUE(result);

  result->Open(ui::EF_NONE);

  // Wait for app service to see the newly launched app.
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->FlushMojoCallsForTesting();

  web_app::WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting(
          [](apps::AppLaunchParams&& params) -> content::WebContents* {
            NOTREACHED();
            return nullptr;
          }));

  result->Open(ui::EF_NONE);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HelpAppSwaSearchBrowserTest,
                         ::testing::Values(CrosapiParam::kDisabled,
                                           CrosapiParam::kEnabled),
                         WithCrosapiParam::ParamToString);

}  // namespace app_list
