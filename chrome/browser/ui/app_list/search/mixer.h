// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_MIXER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_MIXER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"

class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace app_list {

namespace test {
FORWARD_DECLARE_TEST(MixerTest, Publish);
}

class ChipRanker;
class SearchControllerImpl;
class SearchProvider;
class SearchResultRanker;
enum class RankingItemType;

// Mixer collects results from providers, sorts them and publishes them to the
// SearchResults UI model. The targeted results have 6 slots to hold the
// result.
class Mixer {
 public:
  Mixer(AppListModelUpdater* model_updater,
        SearchControllerImpl* search_controller);

  Mixer(const Mixer&) = delete;
  Mixer& operator=(const Mixer&) = delete;

  ~Mixer();

  // Adds a new mixer group. A "soft" maximum of |max_results| results will be
  // chosen from this group (if 0, will allow unlimited results from this
  // group). If there aren't enough results from all groups, more than
  // |max_results| may be chosen from this group. Returns the group's group_id.
  size_t AddGroup(size_t max_results);

  // Associates a provider with a mixer group.
  void AddProviderToGroup(size_t group_id, SearchProvider* provider);

  // Collects the results, sorts and publishes them.
  void MixAndPublish(size_t num_max_results, const std::u16string& query);

  // Sets a SearchResultRanker to re-rank non-app search results before they are
  // published.
  void SetNonAppSearchResultRanker(std::unique_ptr<SearchResultRanker> ranker);

  void InitializeRankers(Profile* profile);

  SearchResultRanker* search_result_ranker() {
    if (!search_result_ranker_)
      return nullptr;
    return search_result_ranker_.get();
  }

  // Sets a ChipRanker to re-rank chip results before they are published.
  void SetChipRanker(std::unique_ptr<ChipRanker> ranker);

  // Handle a training signal.
  void Train(const LaunchData& launch_data);

  // Used for sorting and mixing results.
  struct SortData {
    SortData();
    SortData(ChromeSearchResult* result, double score);

    bool operator<(const SortData& other) const;

    raw_ptr<ChromeSearchResult> result;  // Not owned.
    double score;
  };
  typedef std::vector<Mixer::SortData> SortedResults;

 private:
  FRIEND_TEST_ALL_PREFIXES(test::MixerTest, Publish);

  class Group;
  typedef std::vector<std::unique_ptr<Group>> Groups;

  void FetchResults(const std::u16string& query);

  const raw_ptr<AppListModelUpdater> model_updater_;       // Not owned.
  const raw_ptr<SearchControllerImpl> search_controller_;  // Not owned.

  Groups groups_;

  // Adaptive models used for re-ranking search results.
  std::unique_ptr<SearchResultRanker> search_result_ranker_;
  std::unique_ptr<ChipRanker> chip_ranker_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_MIXER_H_
