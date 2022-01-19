// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_FTRL_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_FTRL_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/util/ftrl_optimizer.h"
#include "chrome/browser/ui/app_list/search/util/mrfu_cache.h"

namespace app_list {

// A ranker for search results using a Follow the Regularized Leader algorithm.
// This learns weightings for the 'experts' below.
class FtrlRanker : public Ranker {
 public:
  enum class RankingKind {
    kResults,
    kCategories,
  };

  FtrlRanker(RankingKind kind,
             FtrlOptimizer::Params params,
             FtrlOptimizer::Proto proto);
  ~FtrlRanker() override;

  FtrlRanker(const FtrlRanker&) = delete;
  FtrlRanker& operator=(const FtrlRanker&) = delete;

  void AddExpert(std::unique_ptr<Ranker> ranker);

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesList& categories) override;
  void Train(const LaunchData& launch) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override;

 private:
  RankingKind kind_;

  // The Follow the Regularized Leader instance that chooses amongst the expert
  // |rankers_|.
  std::unique_ptr<FtrlOptimizer> ftrl_;

  // The 'experts' in the follow-the-regularized-leader model.
  std::vector<std::unique_ptr<Ranker>> rankers_;
};

// An expert that keeps track of usage of individual results using a
// most-recently-frequently-used cache of launch history.
class MrfuResultRanker : public Ranker {
 public:
  explicit MrfuResultRanker(MrfuCache::Params params, MrfuCache::Proto proto);

  ~MrfuResultRanker() override;

  // Ranker:
  std::vector<double> GetResultRanks(const ResultsMap& results,
                                     ProviderType provider) override;
  void Train(const LaunchData& launch) override;

 private:
  std::unique_ptr<MrfuCache> mrfu_;
};

// An expert that exposes the normalized scores from each result's scoring
// struct, set by the ScoreNormalizingRanker.
class NormalizedScoreResultRanker : public Ranker {
 public:
  // Ranker:
  std::vector<double> GetResultRanks(const ResultsMap& results,
                                     ProviderType provider) override;
};

// Ranks a category based on the normalized relevance of its best result.
class BestResultCategoryRanker : public Ranker {
 public:
  BestResultCategoryRanker();
  ~BestResultCategoryRanker() override;

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesList& categories) override;
  std::vector<double> GetCategoryRanks(const ResultsMap& results,
                                       const CategoriesList& categories,
                                       ProviderType provider) override;

 private:
  base::flat_map<Category, double> current_category_scores_;
};

// An expert that keeps track of category usage with a
// most-recently-frequently-used cache of launch history.
class MrfuCategoryRanker : public Ranker {
 public:
  MrfuCategoryRanker(MrfuCache::Params params,
                     PersistentProto<MrfuCacheProto> proto);
  ~MrfuCategoryRanker() override;

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesList& categories) override;
  std::vector<double> GetCategoryRanks(const ResultsMap& results,
                                       const CategoriesList& categories,
                                       ProviderType provider) override;
  void Train(const LaunchData& launch) override;

 private:
  std::unique_ptr<MrfuCache> mrfu_;
  std::vector<double> current_category_scores_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_FTRL_RANKER_H_
