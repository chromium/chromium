// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/category_usage_ranker.h"

#include "base/logging.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using testing::UnorderedElementsAre;

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

class CategoryUsageRankerTest : public testing::Test {
 public:
  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(CategoryUsageRankerTest, DefaultScoresForEmptyRanker) {
  CategoryUsageRanker ranker(profile_.get());
  CategoriesList categories = CreateAllCategories();
  EXPECT_EQ(categories.size(), static_cast<size_t>(Category::kMaxValue));
  ResultsMap results;

  Wait();
  // Ranker is empty, so default scores should fill in the category ranks. Don't
  // check the specifics of the ranking, only that all categories are accounted
  // for.
  ranker.Start(u"", results, categories);
  EXPECT_EQ(categories.size(), static_cast<size_t>(Category::kMaxValue));
}

TEST_F(CategoryUsageRankerTest, TrainIncreasesScore) {
  CategoryUsageRanker ranker(profile_.get());
  Wait();

  // Train on web results.
  {
    LaunchData launch;
    launch.launched_from = ash::AppListLaunchedFrom::kLaunchedFromSearchBox;
    launch.result_type = ResultType::kOmnibox;
    for (int i = 0; i < 10; ++i)
      ranker.Train(launch);
  }

  // Train on apps.
  {
    LaunchData launch;
    launch.launched_from = ash::AppListLaunchedFrom::kLaunchedFromSearchBox;
    launch.result_type = ResultType::kInstalledApp;
    for (int i = 0; i < 10; ++i) {
      ranker.Train(launch);
    }
  }

  // The apps category should rank higher than the web category, but both should
  // be ranked highly.
  CategoriesList categories = CreateAllCategories();
  ResultsMap results;
  ranker.Start(u"", results, categories);

  std::sort(categories.begin(), categories.end(),
            [](const auto& a, const auto& b) { return a.score > b.score; });
  ASSERT_GT(categories.size(), 2u);
  EXPECT_EQ(categories[0].category, Category::kApps);
  EXPECT_EQ(categories[1].category, Category::kWeb);
  EXPECT_GT(categories[1].score, 0.1);
}

}  // namespace app_list
