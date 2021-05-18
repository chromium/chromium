// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/top_match_ranker.h"

#include <vector>

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

namespace app_list {
namespace {

// The maximum number of top matches to show.
constexpr size_t kNumTopMatches = 3u;

// The score threshold before we consider a result a top match.
constexpr double kTopMatchThreshold = 0.9;

// The score added to a results relevance to make it a top match.
constexpr double kScoreBoost = 1000.0;

// Returns true if the |type| provider's results should never be a top match.
bool ShouldIgnoreProvider(ProviderType type) {
  switch (type) {
      // Low-intent providers:
    case ProviderType::kPlayStoreReinstallApp:
    case ProviderType::kPlayStoreApp:
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
      const double current_score = result->relevance();
      if (current_score >= kScoreBoost)
        result->set_relevance(current_score - kScoreBoost);
      if (current_score >= kTopMatchThreshold)
        top_results.push_back({result.get(), current_score});
    }
  }

  // Sort |top_results| best-to-worst.
  std::sort(top_results.begin(), top_results.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  // Apply a score boost to at most the top |kNumTopMatches| results.
  for (int i = 0; i < std::min(kNumTopMatches, top_results.size()); ++i) {
    const double current_score = top_results[i].second;
    top_results[i].first->set_relevance(kScoreBoost + current_score);
  }
}

}  // namespace app_list
