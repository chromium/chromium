// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/top_match_ranker.h"

#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {
namespace {

constexpr char16_t kTopMatchDetails[] = u"(top match) ";

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

void TopMatchRanker::Rank(ResultsMap& results, ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  // TODO(crbug.com/1199206): This is an inefficient way of setting the top
  // matches. Once we have category support built in to the ChromeSearchResult
  // type this should be simplified.

  // Early exit for providers we don't include in the top matches.
  if (ShouldIgnoreProvider(provider))
    return;

  // Build a vector of all current matches. At the same time, reset the top
  // match list by removing the score boost from any results that have it
  // currently.
  std::vector<std::pair<ChromeSearchResult*, double>> top_results;
  for (const auto& type_results : results) {
    for (const auto& result : type_results.second) {
      if (result->relevance() >= kTopMatchScoreBoost)
        result->set_relevance(result->relevance() - kTopMatchScoreBoost);
      if (result->relevance() >= kTopMatchThreshold)
        top_results.push_back({result.get(), result->relevance()});
    }
  }

  // Sort |top_results| best-to-worst.
  std::sort(top_results.begin(), top_results.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  // Apply a score boost to at most the top |kNumTopMatches| results.
  for (int i = 0; i < std::min(kNumTopMatches, top_results.size()); ++i) {
    const double current_score = top_results[i].second;
    top_results[i].first->set_relevance(kTopMatchScoreBoost + current_score);

    // TODO(crbug.com/1199206): This adds some debug information to the result
    // details. Remove once we have explicit categories in the UI.
    const auto details = RemoveDebugPrefix(top_results[i].first->details());
    top_results[i].first->SetDetails(base::StrCat({kTopMatchDetails, details}));
  }
}

}  // namespace app_list
