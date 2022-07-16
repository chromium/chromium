// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_list_search_test_helper.h"
#include "chrome/browser/ui/browser.h"

namespace app_list {

AppListSearchBrowserTest::AppListSearchBrowserTest() {
  scoped_feature_list_.InitWithFeatures(
      {chromeos::features::kHelpAppLauncherSearch,
       chromeos::features::kHelpAppDiscoverTab},
      {});
}

void AppListSearchBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
}

// The AppListClient is in charge of communication from ash to chrome, so can
// be used to mimic UI actions. Examples include starting a search, launching
// a result, or possibly activating a particular view.
AppListClientImpl* AppListSearchBrowserTest::GetClient() {
  auto* client = ::test::GetAppListClient();
  CHECK(client);
  return client;
}

void AppListSearchBrowserTest::StartSearch(const std::string& query) {
  GetClient()->StartSearch(base::ASCIIToUTF16(query));
}

void AppListSearchBrowserTest::SearchAndWaitForProviders(
    const std::string& query,
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

std::vector<ChromeSearchResult*> AppListSearchBrowserTest::PublishedResults() {
  return GetClient()
      ->GetModelUpdaterForTest()
      ->GetPublishedSearchResultsForTest();
}

std::vector<ChromeSearchResult*>
AppListSearchBrowserTest::PublishedResultsForProvider(
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
ChromeSearchResult* AppListSearchBrowserTest::FindResult(
    const std::string& id) {
  for (auto* result : PublishedResults()) {
    if (result->id() == id)
      return result;
  }
  return nullptr;
}

Profile* AppListSearchBrowserTest::GetProfile() {
  return browser()->profile();
}

}  // namespace app_list
