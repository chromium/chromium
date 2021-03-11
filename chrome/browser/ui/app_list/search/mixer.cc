// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/mixer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/chip_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"

namespace app_list {
namespace {

// TODO(crbug.com/1028447): This is a stop-gap until we remove the two-stage
// result adding logic in Mixer::MixAndPublish. Remove this when possible.
void RemoveDuplicates(Mixer::SortedResults* results) {
  Mixer::SortedResults deduplicated;
  deduplicated.reserve(results->size());

  std::set<std::string> seen;
  for (const Mixer::SortData& sort_data : *results) {
    // If a result is intended for display in two views, we will have two
    // results with the same id but different display types. We want to keep
    // both of these, so insert concat(id, display_type).
    const std::string display_type = base::NumberToString(
        static_cast<int>(sort_data.result->display_type()));
    if (!seen.insert(
                 base::JoinString({sort_data.result->id(), display_type}, "-"))
             .second)
      continue;

    deduplicated.emplace_back(sort_data);
  }

  results->swap(deduplicated);
}

}  // namespace

Mixer::SortData::SortData() : result(nullptr), score(0.0) {}

Mixer::SortData::SortData(ChromeSearchResult* result, double score)
    : result(result), score(score) {}

bool Mixer::SortData::operator<(const SortData& other) const {
  // This data precedes (less than) |other| if it has specified display index or
  // higher score.
  ash::SearchResultDisplayIndex index1 = result->display_index();
  ash::SearchResultDisplayIndex index2 = other.result->display_index();
  // The |kUndefined| index is larger than other specified indexes.
  if (index1 != index2)
    return index1 < index2;

  return score > other.score;
}

// Used to group relevant providers together for mixing their results.
class Mixer::Group {
 public:
  explicit Group(size_t max_results) : max_results_(max_results) {}
  ~Group() {}

  void AddProvider(SearchProvider* provider) {
    providers_.emplace_back(provider);
  }

  void FetchResults(SearchResultRanker* ranker) {
    results_.clear();

    for (const SearchProvider* provider : providers_) {
      for (const auto& result : provider->results()) {
        DCHECK(!result->id().empty());

        // We cannot rely on providers to give relevance scores in the range
        // [0.0, 1.0]. Clamp to that range.
        results_.emplace_back(
            result.get(), base::ClampToRange(result->relevance(), 0.0, 1.0));
      }
    }

    if (ranker)
      ranker->Rank(&results_);
    std::sort(results_.begin(), results_.end());
  }

  const SortedResults& results() const { return results_; }

  size_t max_results() const { return max_results_; }

 private:
  typedef std::vector<SearchProvider*> Providers;
  const size_t max_results_;

  Providers providers_;  // Not owned.
  SortedResults results_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

Mixer::Mixer(AppListModelUpdater* model_updater)
    : model_updater_(model_updater) {}
Mixer::~Mixer() = default;

void Mixer::InitializeRankers(Profile* profile,
                              SearchController* search_controller) {
  search_result_ranker_ = std::make_unique<SearchResultRanker>(profile);
  search_result_ranker_->InitializeRankers(search_controller);

  if (app_list_features::IsSuggestedFilesEnabled()) {
    chip_ranker_ = std::make_unique<ChipRanker>(profile);
  }
}

size_t Mixer::AddGroup(size_t max_results) {
  groups_.push_back(std::make_unique<Group>(max_results));
  return groups_.size() - 1;
}

void Mixer::AddProviderToGroup(size_t group_id, SearchProvider* provider) {
  groups_[group_id]->AddProvider(provider);
}

void Mixer::MixAndPublish(size_t num_max_results, const std::u16string& query) {
  FetchResults(query);

  SortedResults results;
  results.reserve(num_max_results);

  // Add results from each group. Limit to the maximum number of results in each
  // group.
  for (const auto& group : groups_) {
    const size_t num_results =
        std::min(group->results().size(), group->max_results());
    results.insert(results.end(), group->results().begin(),
                   group->results().begin() + num_results);
  }

  // Zero state search results: if any search provider won't have any results
  // displayed, but has a high-scoring result that the user hasn't seen many
  // times, replace a to-be-displayed result with it.
  if (query.empty() && search_result_ranker_)
    search_result_ranker_->OverrideZeroStateResults(&results);

  // Chip results: rescore the chip results in line with app results.
  if (query.empty() && chip_ranker_) {
    chip_ranker_->Rank(&results);
  }

  std::sort(results.begin(), results.end());

  const size_t original_size = results.size();
  if (original_size < num_max_results) {
    // We didn't get enough results. Insert all the results again, and this
    // time, do not limit the maximum number of results from each group.
    for (const auto& group : groups_) {
      results.insert(results.end(), group->results().begin(),
                     group->results().end());
    }
    // Sort just the newly added results. This ensures that, for example, if
    // there are 6 Omnibox results (score = 0.8) and 1 People result (score =
    // 0.4) that the People result will be 5th, not 7th, because the Omnibox
    // group has a soft maximum of 4 results. (Otherwise, the People result
    // would not be seen at all once the result list is truncated.)
    std::sort(results.begin() + original_size, results.end());
  }
  RemoveDuplicates(&results);

  std::vector<ChromeSearchResult*> new_results;
  for (const SortData& sort_data : results) {
    sort_data.result->SetDisplayScore(sort_data.score);
    new_results.push_back(sort_data.result);
  }
  model_updater_->PublishSearchResults(new_results);
}

void Mixer::FetchResults(const std::u16string& query) {
  if (search_result_ranker_)
    search_result_ranker_->FetchRankings(query);
  for (const auto& group : groups_)
    group->FetchResults(search_result_ranker_.get());
}

void Mixer::Train(const AppLaunchData& app_launch_data) {
  if (search_result_ranker_)
    search_result_ranker_->Train(app_launch_data);
  if (chip_ranker_)
    chip_ranker_->Train(app_launch_data);
}

}  // namespace app_list
