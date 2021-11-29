// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/top_match_ranker.h"

#include <cmath>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {
namespace {

// Returns true if the |type| provider's results should never be a top match.
bool ShouldIgnoreProvider(ProviderType type) {
  switch (type) {
      // Low-intent providers:
    case ProviderType::kPlayStoreReinstallApp:
    case ProviderType::kPlayStoreApp:
    case ProviderType::kAssistantText:
      // Deprecated providers:
    case ProviderType::kLauncher:
    case ProviderType::kAnswerCard:
      // Suggestion chip results:
    case ProviderType::kFileChip:
    case ProviderType::kDriveChip:
    case ProviderType::kAssistantChip:
      // Internal results:
    case ProviderType::kUnknown:
    case ProviderType::kInternalPrivacyInfo:
      return true;
    default:
      return false;
  }
}

}  // namespace

TopMatchRanker::TopMatchRanker() {}

TopMatchRanker::~TopMatchRanker() {}

void TopMatchRanker::UpdateResultRanks(ResultsMap& results,
                                       ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  // TODO(crbug.com/1199206): This is an inefficient way of setting the top
  // matches. Once we have category support built in to the ChromeSearchResult
  // type this should be simplified.

  // Build a vector of all current matches. At the same time, reset the top
  // match list by removing the score boost from any results that have it
  // currently.
  std::vector<std::pair<ChromeSearchResult*, double>> top_results;
  for (const auto& type_results : results) {
    // Skip results from providers that are never included in the top matches.
    if (ShouldIgnoreProvider(type_results.first))
      continue;

    for (const auto& result : type_results.second) {
      Scoring& scoring = result->scoring();

      // Reset top match status on each result, as it might have previously been
      // set as a top match.
      if (scoring.top_match) {
        scoring.top_match = false;
        result->SetDetails(RemoveTopMatchPrefix(result->details()));
      }

      if (scoring.normalized_relevance >= kTopMatchThreshold)
        top_results.push_back({result.get(), scoring.normalized_relevance});
    }
  }

  // Sort |top_results| best-to-worst according to normalized relevance.
  std::sort(top_results.begin(), top_results.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  for (int i = 0; i < std::min(kNumTopMatches, top_results.size()); ++i) {
    auto* result = top_results[i].first;
    result->scoring().top_match = true;

    // TODO(crbug.com/1199206): This adds some debug information to the result
    // details. Remove once we have explicit categories in the UI.
    result->SetDetails(
        base::StrCat({kTopMatchDetailsUTF16, result->details()}));
  }
}

}  // namespace app_list
