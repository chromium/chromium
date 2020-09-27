// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/launcher_search_provider/launcher_search_provider_service.h"
#include "chromeos/components/string_matching/fuzzy_tokenized_string_match.h"

using chromeos::launcher_search_provider::Service;

namespace app_list {

namespace {

using TokenizedString = chromeos::string_matching::TokenizedString;
using FuzzyTokenizedStringMatch =
    chromeos::string_matching::FuzzyTokenizedStringMatch;

constexpr int kLauncherSearchProviderQueryDelayInMs = 100;
constexpr int kLauncherSearchProviderMaxResults = 6;

constexpr double kDefaultRelevance = 0.5;

// Parameters for FuzzyTokenizedStringMatch. Note that the underlying file
// search uses an exact substring match to retrieve file results, so using edit
// distance here doesn't provide any benefit.
constexpr bool kUsePrefixOnly = false;
constexpr bool kUseWeightedRatio = true;
constexpr bool kUseEditDistance = false;
constexpr double kRelevanceThreshold = 0.0;
constexpr double kPartialMatchPenaltyRate = 0.9;

double FuzzyMatchRelevance(const TokenizedString& title,
                           const TokenizedString& query) {
  if (title.text().empty() || query.text().empty()) {
    return kDefaultRelevance;
  }

  FuzzyTokenizedStringMatch match;
  match.IsRelevant(query, title, kRelevanceThreshold, kUsePrefixOnly,
                   kUseWeightedRatio, kUseEditDistance,
                   kPartialMatchPenaltyRate);
  return match.relevance();
}

}  // namespace

LauncherSearchProvider::LauncherSearchProvider(Profile* profile)
    : profile_(profile) {}

LauncherSearchProvider::~LauncherSearchProvider() {
  Service* service = Service::Get(profile_);
  if (service->IsQueryRunning())
    service->OnQueryEnded();
}

void LauncherSearchProvider::Start(const base::string16& query) {
  query_timer_.Stop();

  // Clear all search results of the previous query. Since results are
  // duplicated when being exported from the map, there are no external pointers
  // to |extension_results_|, so it is safe to clear the map.
  extension_results_.clear();

  Service* service = Service::Get(profile_);

  // Since we delay queries and filter out empty string queries, it can happen
  // that no query is running at service side.
  if (service->IsQueryRunning())
    service->OnQueryEnded();

  // Clear previously added search results.
  ClearResults();

  // LauncherSearchProvider only handles query searches.
  if (query.empty())
    return;

  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);
  DelayQuery(base::Bind(&LauncherSearchProvider::StartInternal,
                        weak_ptr_factory_.GetWeakPtr(), query));
}

void LauncherSearchProvider::SetSearchResults(
    const extensions::ExtensionId& extension_id,
    std::vector<std::unique_ptr<LauncherSearchResult>> results) {
  DCHECK(Service::Get(profile_)->IsQueryRunning());

  // Record file search query latency metrics.
  UMA_HISTOGRAM_TIMES("Apps.AppList.LauncherSearchProvider.QueryTime",
                      base::TimeTicks::Now() - query_start_time_);

  // Add this extension's results (erasing any existing results).
  extension_results_[extension_id] = std::move(results);
  DCHECK_LE(extension_results_.size(), 1);

  // Update results with other extension results.
  SearchProvider::Results new_results;
  for (const auto& item : extension_results_) {
    for (const auto& result : item.second) {
      std::unique_ptr<LauncherSearchResult> new_result = result->Duplicate();

      double relevance = kDefaultRelevance;
      if (last_tokenized_query_) {
        const TokenizedString tokenized_title(new_result->title(),
                                              TokenizedString::Mode::kWords);
        relevance =
            FuzzyMatchRelevance(tokenized_title, last_tokenized_query_.value());
      }
      new_result->set_relevance(relevance);

      new_results.push_back(std::move(new_result));
    }
  }
  SwapResults(&new_results);
}

ash::AppListSearchResultType LauncherSearchProvider::ResultType() {
  return ash::AppListSearchResultType::kLauncher;
}

void LauncherSearchProvider::DelayQuery(const base::Closure& closure) {
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(kLauncherSearchProviderQueryDelayInMs);
  if (base::Time::Now() - last_query_time_ > delay) {
    query_timer_.Stop();
    closure.Run();
  } else {
    query_timer_.Start(FROM_HERE, delay, closure);
  }
  last_query_time_ = base::Time::Now();
}

void LauncherSearchProvider::StartInternal(const base::string16& query) {
  if (!query.empty()) {
    query_start_time_ = base::TimeTicks::Now();
    Service::Get(profile_)->OnQueryStarted(this, base::UTF16ToUTF8(query),
                                           kLauncherSearchProviderMaxResults);
  }
}

}  // namespace app_list
