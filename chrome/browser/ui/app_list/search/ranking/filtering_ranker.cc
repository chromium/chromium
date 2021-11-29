// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/filtering_ranker.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {

FilteringRanker::FilteringRanker() {}

FilteringRanker::~FilteringRanker() {}

void FilteringRanker::Start(const std::u16string& query,
                            ResultsMap& results,
                            CategoriesList& categories) {
  last_query_ = query;
}

void FilteringRanker::UpdateResultRanks(ResultsMap& results,
                                        ProviderType provider) {
  // Don't perform any filtering on zero-state.
  if (last_query_.empty())
    return;

  // We currently only filter omnibox results. So if we don't have any yet,
  // early exit.
  const auto it = results.find(ProviderType::kOmnibox);
  if (it == results.end())
    return;

  // Compute the total number of results. If we have fewer than can fit in the
  // UI, early exit.
  static const int max_search_results =
      ash::SharedAppListConfig::instance().max_search_results();
  int total_results = 0;
  for (const auto& type_results : results)
    total_results += type_results.second.size();
  if (total_results <= max_search_results)
    return;

  // Sort the list of omnibox results best-to-worst.
  auto& omnibox_results = results[ProviderType::kOmnibox];
  std::sort(omnibox_results.begin(), omnibox_results.end(),
            [](const auto& a, const auto& b) {
              return a->relevance() > b->relevance();
            });

  // Filter all results after the |kMaxOmniboxResults|th one out of the UI, but
  // never remove top matches.
  for (int i = kMaxOmniboxResults; i < omnibox_results.size(); ++i) {
    auto& scoring = omnibox_results[i]->scoring();
    if (!scoring.top_match)
      scoring.filter = true;
  }
}

}  // namespace app_list
