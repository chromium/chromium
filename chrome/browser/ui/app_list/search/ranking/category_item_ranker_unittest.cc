// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/category_item_ranker.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using testing::Contains;

MATCHER_P2(Metadata, category, score, "") {
  bool match = arg.category == category && arg.score == score;
  if (!match)
    *result_listener << "Metadata wants (" << static_cast<size_t>(category)
                     << ", " << score << "), but got ("
                     << static_cast<size_t>(arg.category) << ", " << arg.score
                     << ")";
  return match;
}

class TestResult : public ChromeSearchResult {
 public:
  TestResult(double relevance, Category category, bool best_match) {
    // For testing, directly set the normalized relevance. A normal result
    // wouldn't do this.
    scoring().normalized_relevance = relevance;
    SetCategory(category);
    SetBestMatch(best_match);
  }
  ~TestResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

Results make_results(Category category,
                     std::vector<double> relevances,
                     std::vector<bool> best_matches) {
  Results res;
  for (size_t i = 0; i < relevances.size(); ++i) {
    res.push_back(
        std::make_unique<TestResult>(relevances[i], category, best_matches[i]));
  }
  return res;
}

}  // namespace

TEST(CategoryItemRankerTest, UpdatesScores) {
  ResultsMap results;
  results[ResultType::kInstalledApp] =
      make_results(Category::kApps, {0.1, 1.5, 0.9}, {false, false, false});
  results[ResultType::kInternalApp] =
      make_results(Category::kApps, {10.0, 2.0}, {true, false});
  results[ResultType::kOmnibox] =
      make_results(Category::kWeb, {0.3, 0.4, 0.3}, {false, true, false});

  CategoryItemRanker ranker;
  CategoriesList categories = CreateAllCategories();

  // Only the kInstalledApp results should be used in this call to Rank.
  ranker.UpdateCategoryRanks(results, categories, ResultType::kInstalledApp);
  EXPECT_THAT(categories, Contains(Metadata(Category::kApps, 1.5)));

  // Only the kInternalApp results should be used, but the best match ignored.
  ranker.UpdateCategoryRanks(results, categories, ResultType::kInternalApp);
  EXPECT_THAT(categories, Contains(Metadata(Category::kApps, 2.0)));

  // Ranking a new category should preserve the old ranking.
  ranker.UpdateCategoryRanks(results, categories, ResultType::kOmnibox);
  EXPECT_THAT(categories, Contains(Metadata(Category::kApps, 2.0)));
  EXPECT_THAT(categories, Contains(Metadata(Category::kWeb, 0.3)));
}

}  // namespace app_list
