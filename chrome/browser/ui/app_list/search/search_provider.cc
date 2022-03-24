// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_provider.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {

SearchProvider::SearchProvider() {}
SearchProvider::~SearchProvider() {}

void SearchProvider::Add(std::unique_ptr<ChromeSearchResult> result) {
  results_.emplace_back(std::move(result));
  FireResultChanged();
}

// TODO(crbug.com/1199206): As part of the change to category-based search,
// the method of updating the search controller is being changed. Once
// categorical search is enabled, we should clean up the SearchProvider
// interface.

void SearchProvider::SwapResults(Results* new_results) {
  if (app_list_features::IsCategoricalSearchEnabled()) {
    Results results;
    results.swap(*new_results);
    if (search_controller_)
      search_controller_->SetResults(this, std::move(results));
    FireResultChanged();
  } else {
    results_.swap(*new_results);
    FireResultChanged();
  }
}

void SearchProvider::ClearResults() {
  if (!app_list_features::IsCategoricalSearchEnabled()) {
    results_.clear();
    FireResultChanged();
  }
}

void SearchProvider::ClearResultsSilently() {
  if (!app_list_features::IsCategoricalSearchEnabled()) {
    results_.clear();
  }
}

void SearchProvider::FireResultChanged() {
  if (result_changed_callback_.is_null())
    return;

  result_changed_callback_.Run();
}

bool SearchProvider::ShouldBlockZeroState() const {
  return false;
}

}  // namespace app_list
