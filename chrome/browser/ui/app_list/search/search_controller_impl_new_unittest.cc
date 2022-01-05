// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller_impl_new.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"

namespace app_list {
namespace {

// TODO(crbug.com/1258415): Since we have a lot of class fakes now, we should
// generalize them and split them into a test utils directory.

using Category = ash::AppListSearchResultCategory;
using Result = ash::AppListSearchResultType;

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id,
                   Category category,
                   bool best_match,
                   double relevance) {
    set_id(id);
    SetCategory(category);
    SetBestMatch(best_match);
    set_relevance(relevance);
    scoring().normalized_relevance = relevance;
  }

  TestSearchResult(const TestSearchResult&) = delete;
  TestSearchResult& operator=(const TestSearchResult&) = delete;
  ~TestSearchResult() override = default;

 private:
  void Open(int event_flags) override { NOTIMPLEMENTED(); }
};

class TestSearchProvider : public SearchProvider {
 public:
  TestSearchProvider(ash::AppListSearchResultType result_type,
                     bool block_zero_state,
                     base::TimeDelta delay)
      : result_type_(result_type),
        block_zero_state_(block_zero_state),
        delay_(delay) {}

  ~TestSearchProvider() override = default;

  void SetNextResults(
      std::vector<std::unique_ptr<ChromeSearchResult>> results) {
    results_ = std::move(results);
  }

  bool ShouldBlockZeroState() const override { return block_zero_state_; }

  ash::AppListSearchResultType ResultType() const override {
    return result_type_;
  }

  void Start(const std::u16string& query) override {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestSearchProvider::SetResults, base::Unretained(this)),
        delay_);
  }

  void StartZeroState() override {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestSearchProvider::SetResults, base::Unretained(this)),
        delay_);
  }

 private:
  void SetResults() { SwapResults(&results_); }

  std::vector<std::unique_ptr<ChromeSearchResult>> results_;
  ash::AppListSearchResultType result_type_;
  bool block_zero_state_;
  base::TimeDelta delay_;
};

// A test ranker delegate that circumvents all result rankings, and hardcodes
// category ranking.
class TestRankerDelegate : public RankerDelegate {
 public:
  explicit TestRankerDelegate(Profile* profile)
      : RankerDelegate(profile, nullptr) {}
  ~TestRankerDelegate() override {}

  TestRankerDelegate(const TestRankerDelegate&) = delete;
  TestRankerDelegate& operator=(const TestRankerDelegate&) = delete;

  void SetCategoryRanks(base::flat_map<Category, double> category_ranks) {
    category_ranks_ = category_ranks;
  }

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override {
    // Noop.
  }

  // Ranker:
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override {
    for (auto& category : categories) {
      const auto it = category_ranks_.find(category.category);
      if (it != category_ranks_.end())
        category.score = it->second;
    }
  }

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesList& categories) override {}
  void Train(const LaunchData& launch) override {}
  void Remove(ChromeSearchResult* result) override {}

 private:
  base::flat_map<Category, double> category_ranks_;
};

std::vector<std::unique_ptr<ChromeSearchResult>> MakeResults(
    std::vector<std::string> ids,
    std::vector<Category> categories,
    std::vector<bool> best_matches,
    std::vector<double> scores) {
  std::vector<std::unique_ptr<ChromeSearchResult>> results;
  for (size_t i = 0; i < ids.size(); ++i) {
    results.emplace_back(std::make_unique<TestSearchResult>(
        ids[i], categories[i], best_matches[i], scores[i]));
  }
  return results;
}

// Returns a pointer to a search provider. Only valid until the next call to
// SimpleProvider.
static std::unique_ptr<SearchProvider> kProvider;
SearchProvider* SimpleProvider(ash::AppListSearchResultType result_type) {
  kProvider = std::make_unique<TestSearchProvider>(result_type, false,
                                                   base::Seconds(0));
  return kProvider.get();
}

}  // namespace

class SearchControllerImplNewTest : public testing::Test {
 public:
  SearchControllerImplNewTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  SearchControllerImplNewTest(const SearchControllerImplNewTest&) = delete;
  SearchControllerImplNewTest& operator=(const SearchControllerImplNewTest&) =
      delete;
  ~SearchControllerImplNewTest() override = default;

  void SetUp() override {
    // TODO(crbug.com/1258415): Feature list can be removed after launch.
    scoped_feature_list_.InitWithFeatures(
        {app_list_features::kCategoricalSearch}, {});

    search_controller_ = std::make_unique<SearchControllerImplNew>(
        /*model_updater=*/&model_updater_, /*list_controller=*/nullptr,
        /*notifier=*/nullptr, &profile_);

    auto ranker_delegate = std::make_unique<TestRankerDelegate>(&profile_);
    ranker_delegate_ = ranker_delegate.get();
    search_controller_->set_ranker_delegate_for_test(
        std::move(ranker_delegate));
  }

  void ExpectIdOrder(std::vector<std::string> id_order) {
    const auto& sorted_results = model_updater_.search_results();
    ASSERT_EQ(sorted_results.size(), id_order.size());
    for (size_t i = 0; i < sorted_results.size(); ++i)
      EXPECT_EQ(sorted_results[i]->id(), id_order[i]);
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void ElapseBurnInPeriod() {
    task_environment_.FastForwardBy(base::Milliseconds(200));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  FakeAppListModelUpdater model_updater_{&profile_, /*order_delegate=*/nullptr};
  std::unique_ptr<SearchControllerImplNew> search_controller_;
  // Owned by |search_controller_|.
  TestRankerDelegate* ranker_delegate_{nullptr};
};

// Tests that categories are ordered correctly, and their results are grouped
// together and ordered by score.
TEST_F(SearchControllerImplNewTest, CategoriesOrderedCorrectly) {
  ranker_delegate_->SetCategoryRanks(
      {{Category::kFiles, 0.3}, {Category::kWeb, 0.2}, {Category::kApps, 0.1}});
  auto file_results = MakeResults({"a"}, {Category::kFiles}, {false}, {0.9});
  auto web_results = MakeResults(
      {"c", "d", "b"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {false, false, false}, {0.2, 0.1, 0.4});
  auto app_results = MakeResults({"e"}, {Category::kApps}, {false}, {0.1});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");
  ElapseBurnInPeriod();
  // Simulate several providers returning results.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results));
  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(app_results));
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(file_results));

  ExpectIdOrder({"a", "b", "c", "d", "e"});
}

// Tests that best matches are ordered first, and categories are ignored when
// ranking within best match.
TEST_F(SearchControllerImplNewTest, BestMatchesOrderedAboveOtherResults) {
  auto results = MakeResults(
      {"a", "b", "c", "d"},
      {Category::kWeb, Category::kWeb, Category::kApps, Category::kWeb},
      {true, false, true, false}, {0.4, 0.8, 0.2, 0.9});
  ranker_delegate_->SetCategoryRanks(
      {{Category::kApps, 0.4}, {Category::kWeb, 0.2}});

  search_controller_->StartSearch(u"abc");
  ElapseBurnInPeriod();
  // Simulate a provider returning and containing all of the above results. A
  // single provider wouldn't return many results like this, but that's
  // unimportant for the test.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(results));

  ExpectIdOrder({"a", "c", "d", "b"});
}

TEST_F(SearchControllerImplNewTest, SetResultsPreAndPostBurnIn) {
  ranker_delegate_->SetCategoryRanks(
      {{Category::kFiles, 0.3}, {Category::kWeb, 0.2}, {Category::kApps, 0.1}});
  auto file_results = MakeResults({"a"}, {Category::kFiles}, {false}, {0.9});
  auto web_results = MakeResults(
      {"c", "d", "b"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {false, false, false}, {0.2, 0.1, 0.4});
  auto app_results = MakeResults({"e"}, {Category::kApps}, {false}, {0.1});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");

  // Simulate a provider returning results within the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results));
  ExpectIdOrder({});

  // Expect results to appear after burn-in period has elapsed.
  ElapseBurnInPeriod();
  ExpectIdOrder({"b", "c", "d"});

  // Simulate several providers returning results after the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(app_results));
  ExpectIdOrder({"b", "c", "d", "e"});
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(file_results));
  ExpectIdOrder({"a", "b", "c", "d", "e"});
}

TEST_F(SearchControllerImplNewTest, ZeroStateResultsAreBlocked) {
  ranker_delegate_->SetCategoryRanks({{Category::kApps, 0.1}});

  // Set up four providers, two are zero-state blocking. One is slow. The
  // particular result types and categories don't matter.
  auto provider_a = std::make_unique<TestSearchProvider>(
      Result::kInstalledApp, true, base::Seconds(1));
  auto provider_b = std::make_unique<TestSearchProvider>(
      Result::kZeroStateFile, true, base::Seconds(2));
  auto provider_c = std::make_unique<TestSearchProvider>(
      Result::kOsSettings, false, base::Seconds(1));
  auto provider_d = std::make_unique<TestSearchProvider>(
      Result::kOmnibox, false, base::Seconds(4));

  provider_a->SetNextResults(
      MakeResults({"a"}, {Category::kApps}, {false}, {0.3}));
  provider_b->SetNextResults(
      MakeResults({"b"}, {Category::kApps}, {false}, {0.2}));
  provider_c->SetNextResults(
      MakeResults({"c"}, {Category::kApps}, {false}, {0.1}));
  provider_d->SetNextResults(
      MakeResults({"d"}, {Category::kApps}, {false}, {0.4}));

  search_controller_->AddProvider(0, std::move(provider_a));
  search_controller_->AddProvider(0, std::move(provider_b));
  search_controller_->AddProvider(0, std::move(provider_c));
  search_controller_->AddProvider(0, std::move(provider_d));

  // Start the zero-state session. When on-done is called, we should have
  // results from all but the slowest provider.
  search_controller_->StartZeroState(base::BindLambdaForTesting([&]() {
                                       ExpectIdOrder({"a", "b", "c"});
                                     }),
                                     base::Seconds(3));

  // The fast provider has returned but shouldn't have published.
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectIdOrder({});

  // Additionally, those three results should be returned before the
  // StartZeroState timeout.
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectIdOrder({"a", "b", "c"});

  // The latecomer should still be added when it arrives.
  task_environment_.FastForwardBy(base::Seconds(2));
  ExpectIdOrder({"d", "a", "b", "c"});
}

TEST_F(SearchControllerImplNewTest, ZeroStateResultsGetTimedOut) {
  ranker_delegate_->SetCategoryRanks({{Category::kApps, 0.1}});

  auto provider_a = std::make_unique<TestSearchProvider>(
      Result::kInstalledApp, true, base::Seconds(1));
  auto provider_b = std::make_unique<TestSearchProvider>(
      Result::kZeroStateFile, true, base::Seconds(3));

  provider_a->SetNextResults(
      MakeResults({"a"}, {Category::kApps}, {false}, {0.3}));
  provider_b->SetNextResults(
      MakeResults({"b"}, {Category::kFiles}, {false}, {0.2}));

  search_controller_->AddProvider(0, std::move(provider_a));
  search_controller_->AddProvider(0, std::move(provider_b));

  search_controller_->StartZeroState(
      base::BindLambdaForTesting([&]() { ExpectIdOrder({"a"}); }),
      base::Seconds(2));

  // The fast provider has returned but shouldn't have published.
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectIdOrder({});

  // The timeout finished, the fast provider's result should be published.
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectIdOrder({"a"});

  // The slow provider should still publish when it returns.
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectIdOrder({"a", "b"});
}

}  // namespace app_list
