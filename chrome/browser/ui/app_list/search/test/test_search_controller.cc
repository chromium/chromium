// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"

#include "chrome/browser/ui/app_list/search/search_provider.h"

namespace app_list {

TestSearchController::TestSearchController() = default;
TestSearchController::~TestSearchController() = default;

void TestSearchController::StartSearch(const std::u16string& query) {}

void TestSearchController::StartZeroState(base::OnceClosure on_done,
                                          base::TimeDelta timeout) {}

void TestSearchController::ViewClosing() {}

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
    std::unique_ptr<SearchProvider> provider) {}

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

void TestSearchController::AppListShown() {}

int TestSearchController::GetLastQueryLength() const {
  return 0;
}

void TestSearchController::OnSearchResultsImpressionMade(
    const std::u16string& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int launched_index) {}

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
