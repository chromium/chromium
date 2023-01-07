// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

namespace app_list {

TestSearchController::TestSearchController() = default;
TestSearchController::~TestSearchController() = default;

void TestSearchController::StartSearch(const std::u16string& query) {
  // The search controller used when categorical search is enabled clears all
  // results when starging another search query - simulate this behavior in
  // tests when categorical search is enabled.
  if (!ash::IsContinueSectionResultType(provider_->ResultType()) &&
      app_list_features::IsCategoricalSearchEnabled()) {
    last_results_.clear();
  }
  provider_->Start(query);
}

void TestSearchController::StartZeroState(base::OnceClosure on_done,
                                          base::TimeDelta timeout) {
  // The search controller used when categorical search is enabled clears all
  // results when starging another search query - simulate this behavior in
  // tests when categorical search is enabled.
  if (app_list_features::IsCategoricalSearchEnabled())
    last_results_.clear();
  provider_->StartZeroState();
}

void TestSearchController::AppListClosing() {}

void TestSearchController::OpenResult(ChromeSearchResult* result,
                                      int event_flags) {}

void TestSearchController::InvokeResultAction(
    ChromeSearchResult* result,
    ash::SearchResultActionType action) {}

size_t TestSearchController::AddGroup(size_t max_results) {
  return 0u;
}

void TestSearchController::AddProvider(
    size_t group_id,
    std::unique_ptr<SearchProvider> provider) {
  DCHECK(!provider_);
  provider_ = std::move(provider);
  provider_->set_controller(this);
}

void TestSearchController::SetResults(const SearchProvider* provider,
                                      Results results) {
  last_results_ = std::move(results);
}

void TestSearchController::Publish() {}

ChromeSearchResult* TestSearchController::FindSearchResult(
    const std::string& result_id) {
  return nullptr;
}

ChromeSearchResult* TestSearchController::GetResultByTitleForTest(
    const std::string& title) {
  return nullptr;
}

void TestSearchController::Train(LaunchData&& launch_data) {}

void TestSearchController::AddObserver(Observer* observer) {}

void TestSearchController::RemoveObserver(Observer* observer) {}

std::u16string TestSearchController::get_query() {
  return u"";
}

base::Time TestSearchController::session_start() {
  return base::Time::Now();
}

void TestSearchController::set_results_changed_callback_for_test(
    ResultsChangedCallback callback) {}

void TestSearchController::disable_ranking_for_test() {}

}  // namespace app_list
