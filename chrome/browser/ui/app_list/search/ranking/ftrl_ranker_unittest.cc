// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ftrl_ranker.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using testing::ElementsAre;

class TestResult : public ChromeSearchResult {
 public:
  explicit TestResult(const std::string& id,
                      double score,
                      ResultType result_type = ResultType::kUnknown,
                      Category category = Category::kUnknown) {
    set_id(id);
    SetDisplayScore(score);
    scoring().normalized_relevance = score;
    SetResultType(result_type);
    SetCategory(category);
  }
  ~TestResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

class TestRanker : public Ranker {
 public:
  TestRanker() = default;
  ~TestRanker() override = default;

  void SetNextScores(const std::vector<double>& scores) {
    next_scores_ = scores;
  }

  // Ranker:
  std::vector<double> GetResultRanks(const ResultsMap& results,
                                     ProviderType provider) override {
    return next_scores_;
  }

  std::vector<double> GetCategoryRanks(const ResultsMap& results,
                                       const CategoriesList& categories,
                                       ProviderType provider) override {
    return next_scores_;
  }

 private:
  std::vector<double> next_scores_;
};

// A helper function for creating results. For convenience, the provided scores
// are set as both the display score and normalized relevance.
Results MakeScoredResults(const std::vector<std::string>& ids,
                          const std::vector<double> scores,
                          ResultType result_type = ResultType::kUnknown,
                          Category category = Category::kUnknown) {
  Results res;
  CHECK_EQ(ids.size(), scores.size());
  for (size_t i = 0; i < ids.size(); ++i) {
    res.push_back(
        std::make_unique<TestResult>(ids[i], scores[i], result_type, category));
  }
  return res;
}

// A helper function for creating results, for when results don't need scores.
Results MakeResults(const std::vector<std::string>& ids,
                    ResultType result_type = ResultType::kUnknown,
                    Category category = Category::kUnknown) {
  return MakeScoredResults(ids, std::vector<double>(ids.size()), result_type,
                           category);
}

LaunchData MakeLaunchData(const std::string& id,
                          ResultType result_type = ResultType::kUnknown) {
  LaunchData launch;
  launch.launched_from = ash::AppListLaunchedFrom::kLaunchedFromSearchBox;
  launch.id = id;
  launch.result_type = result_type;
  return launch;
}

}  // namespace

class RankerTestBase : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  template <typename T>
  T ReadProtoFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    T proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void Wait() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

// FtrlRankerTest --------------------------------------------------------------

class FtrlRankerTest : public RankerTestBase {
 public:
  FtrlOptimizer::Params TestingParams(size_t num_experts) {
    FtrlOptimizer::Params params;
    params.alpha = 1.0;
    params.gamma = 0.1;
    params.num_experts = num_experts;
    return params;
  }
};

TEST_F(FtrlRankerTest, TrainAndRankResults) {
  // Set up an FTRL result ranker with two experts that statically return scores
  // for four items.
  auto good_ranker = std::make_unique<TestRanker>();
  auto bad_ranker = std::make_unique<TestRanker>();
  good_ranker->SetNextScores({4.0, 3.0, 2.0, 1.0});
  bad_ranker->SetNextScores({1.0, 2.0, 3.0, 4.0});

  FtrlRanker ranker(FtrlRanker::RankingKind::kResults, TestingParams(2),
                    FtrlOptimizer::Proto(GetPath(), base::Seconds(0)));
  ranker.AddExpert(std::move(good_ranker));
  ranker.AddExpert(std::move(bad_ranker));
  Wait();

  // Make four results and mimic several rank/train cycles where we always
  // launch "a", so the good/bad rankers always performs well/poorly.
  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeResults({"a", "b", "c", "d"});

  for (int i = 0; i < 10; ++i) {
    ranker.UpdateResultRanks(results, ResultType::kInstalledApp);
    ranker.Train(MakeLaunchData("a"));
  }

  // The weights of the FTRL optimizer should reflect that the good ranker is
  // better than the bad ranker.
  Wait();
  auto proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2u);
  EXPECT_GE(proto.weights()[0], 0.9);
  EXPECT_LE(proto.weights()[1], 0.1);

  // Now reverse the situation: change the clicked result so the bad ranker is
  // now performing well.
  for (int i = 0; i < 10; ++i) {
    ranker.UpdateResultRanks(results, ResultType::kInstalledApp);
    ranker.Train(MakeLaunchData("d"));
  }

  // The weights of the 'bad' expert should have recovered.
  Wait();
  proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2u);
  EXPECT_LE(proto.weights()[0], 0.1);
  EXPECT_GE(proto.weights()[1], 0.9);
}

TEST_F(FtrlRankerTest, TrainAndRankCategories) {
  auto good_ranker = std::make_unique<TestRanker>();
  auto bad_ranker = std::make_unique<TestRanker>();
  good_ranker->SetNextScores({4.0, 3.0, 2.0, 1.0});
  bad_ranker->SetNextScores({1.0, 2.0, 3.0, 4.0});

  FtrlRanker ranker(FtrlRanker::RankingKind::kCategories, TestingParams(2),
                    FtrlOptimizer::Proto(GetPath(), base::Seconds(0)));
  ranker.AddExpert(std::move(good_ranker));
  ranker.AddExpert(std::move(bad_ranker));
  Wait();

  // Make results in three categories (assistant has no results) and mimic
  // several rank/train cycles where we always launch "a", so the good/bad
  // rankers always performs well/poorly.
  ResultsMap results;
  results[ResultType::kInstalledApp] =
      MakeResults({"a"}, ResultType::kInstalledApp, Category::kApps);
  results[ResultType::kOsSettings] =
      MakeResults({"b"}, ResultType::kOsSettings, Category::kSettings);
  results[ResultType::kFileSearch] =
      MakeResults({"c"}, ResultType::kFileSearch, Category::kFiles);
  CategoriesList categories({{.category = Category::kApps},
                             {.category = Category::kSettings},
                             {.category = Category::kSearchAndAssistant},
                             {.category = Category::kFiles}});

  for (int i = 0; i < 10; ++i) {
    ranker.UpdateCategoryRanks(results, categories, ResultType::kInstalledApp);
    ranker.Train(MakeLaunchData("a", ResultType::kInstalledApp));
  }

  // The weights of the FTRL optimizer should reflect that the good ranker is
  // better than the bad ranker.
  Wait();
  auto proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2u);
  EXPECT_GE(proto.weights()[0], 0.9);
  EXPECT_LE(proto.weights()[1], 0.1);

  // Now reverse the situation: change the clicked result so the bad ranker is
  // now performing well. Train for longer because neither ranker does a
  // particularly good job at predicting "c"'s category.
  for (int i = 0; i < 10; ++i) {
    ranker.UpdateResultRanks(results, ResultType::kInstalledApp);
    ranker.Train(MakeLaunchData("c", ResultType::kFileSearch));
  }

  // The weights of the 'bad' expert should have recovered.
  Wait();
  proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2u);
  EXPECT_LE(proto.weights()[0], 0.1);
  EXPECT_GE(proto.weights()[1], 0.9);
}

// MrfuResultRanker ------------------------------------------------------------

class MrfuResultRankerTest : public RankerTestBase {};

TEST_F(MrfuResultRankerTest, TrainAndRank) {
  MrfuResultRanker ranker(MrfuCache::Params(),
                          MrfuCache::Proto(GetPath(), base::Seconds(0)));
  Wait();

  // Train on some results.
  ranker.Train(MakeLaunchData("a"));
  ranker.Train(MakeLaunchData("b"));
  ranker.Train(MakeLaunchData("c"));
  ranker.Train(MakeLaunchData("a"));

  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeResults({"a", "b"});
  results[ResultType::kOsSettings] = MakeResults({"c", "d"});
  CategoriesList categories;

  // Rank them, expecting ordering a > c > b > d.
  ranker.Start(u"query", results, categories);
  auto ab_scores = ranker.GetResultRanks(results, ResultType::kInstalledApp);
  auto cd_scores = ranker.GetResultRanks(results, ResultType::kOsSettings);
  EXPECT_GT(ab_scores[0], cd_scores[0]);
  EXPECT_GT(cd_scores[0], ab_scores[1]);
  EXPECT_GT(ab_scores[1], cd_scores[1]);
}

// NormalizedScoreResultRanker -------------------------------------------------

class NormalizedScoreResultRankerTest : public RankerTestBase {};

TEST_F(NormalizedScoreResultRankerTest, Rank) {
  NormalizedScoreResultRanker ranker;

  auto results = MakeResults({"a", "b", "c"});
  ASSERT_EQ(results.size(), 3u);
  results[0]->scoring().normalized_relevance = 0.2;
  results[1]->scoring().normalized_relevance = 0.5;
  results[2]->scoring().normalized_relevance = 0.1;

  ResultsMap results_map;
  results_map[ResultType::kInstalledApp] = std::move(results);

  auto scores = ranker.GetResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_EQ(scores[0], 0.2);
  EXPECT_EQ(scores[1], 0.5);
  EXPECT_EQ(scores[2], 0.1);
}

// BestResultCategoryRanker ----------------------------------------------------

class BestResultCategoryRankerTest : public RankerTestBase {};

TEST_F(BestResultCategoryRankerTest, Rank) {
  BestResultCategoryRanker ranker;

  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeScoredResults(
      {"a", "b"}, {0.1, 0.5}, ResultType::kInstalledApp, Category::kApps);
  results[ResultType::kOsSettings] = MakeScoredResults(
      {"c", "d"}, {0.3, 0.3}, ResultType::kOsSettings, Category::kSettings);
  results[ResultType::kFileSearch] = MakeScoredResults(
      {"e", "f"}, {0.3, 0.8}, ResultType::kFileSearch, Category::kFiles);
  CategoriesList categories({{.category = Category::kApps},
                             {.category = Category::kSettings},
                             {.category = Category::kFiles}});

  ranker.Start(u"query", results, categories);

  // Only the app category should be scored, because that's the only one to have
  // returned.
  auto scores =
      ranker.GetCategoryRanks(results, categories, ResultType::kInstalledApp);
  ASSERT_EQ(scores.size(), 3u);
  EXPECT_EQ(scores[0], 0.5);
  EXPECT_EQ(scores[1], 0.0);
  EXPECT_EQ(scores[2], 0.0);

  // Now all categories should be scored.
  ranker.GetCategoryRanks(results, categories, ResultType::kOsSettings);
  scores =
      ranker.GetCategoryRanks(results, categories, ResultType::kFileSearch);
  ASSERT_EQ(scores.size(), 3u);
  EXPECT_EQ(scores[0], 0.5);
  EXPECT_EQ(scores[1], 0.3);
  EXPECT_EQ(scores[2], 0.8);
}

// MrfuCategoryRanker ----------------------------------------------------------

class MrfuCategoryRankerTest : public RankerTestBase {};

TEST_F(MrfuCategoryRankerTest, TrainAndRank) {
  MrfuCategoryRanker ranker(MrfuCache::Params(),
                            MrfuCache::Proto(GetPath(), base::Seconds(0)));
  Wait();

  // Train so that settings should be first, followed by apps.
  ranker.Train(MakeLaunchData("a", ResultType::kInstalledApp));
  ranker.Train(MakeLaunchData("c", ResultType::kOsSettings));
  ranker.Train(MakeLaunchData("d", ResultType::kOsSettings));
  ranker.Train(MakeLaunchData("b", ResultType::kInstalledApp));

  ResultsMap results;
  results[ResultType::kInstalledApp] =
      MakeResults({"a", "b"}, ResultType::kInstalledApp, Category::kApps);
  results[ResultType::kOsSettings] =
      MakeResults({"c", "d"}, ResultType::kOsSettings, Category::kSettings);
  results[ResultType::kFileSearch] =
      MakeResults({"e", "f"}, ResultType::kFileSearch, Category::kFiles);
  CategoriesList categories({{.category = Category::kApps},
                             {.category = Category::kSettings},
                             {.category = Category::kFiles}});

  // Expect a ranking of kInstalledApp > kOsSettings > kFileSearch.
  ranker.Start(u"query", results, categories);
  auto scores =
      ranker.GetCategoryRanks(results, categories, ResultType::kInstalledApp);
  ASSERT_EQ(scores.size(), 3u);
  EXPECT_GT(scores[0], scores[1]);
  EXPECT_GT(scores[1], scores[2]);
}

}  // namespace app_list
