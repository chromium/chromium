// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history/core/test/test_history_database.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using ResultType = ash::AppListSearchResultType;

using base::ScopedTempDir;
using base::test::ScopedFeatureList;
using testing::ElementsAre;
using testing::StrEq;
using testing::UnorderedElementsAre;
using testing::WhenSorted;

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, ResultType type)
      : instance_id_(instantiation_count++) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    SetResultType(type);
  }
  ~TestSearchResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}

 private:
  static int instantiation_count;

  int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};
int TestSearchResult::instantiation_count = 0;

MATCHER_P(HasId, id, "") {
  bool match = base::UTF16ToUTF8(arg.result->title()) == id;
  if (!match)
    *result_listener << "HasId wants '" << id << "', but got '"
                     << arg.result->title() << "'";
  return match;
}

MATCHER_P2(HasIdScore, id, score, "") {
  bool match =
      base::UTF16ToUTF8(arg.result->title()) == id && arg.score == score;
  if (!match)
    *result_listener << "HasIdScore wants (" << id << ", " << score
                     << "), but got (" << arg.result->title() << ", "
                     << arg.result->title() << ")";
  return match;
}

std::unique_ptr<KeyedService> BuildHistoryService(
    content::BrowserContext* context) {
  TestingProfile* profile = static_cast<TestingProfile*>(context);

  base::FilePath history_path(profile->GetPath().Append("history"));

  // Delete the file before creating the service.
  if (!base::DeleteFile(history_path) || base::PathExists(history_path)) {
    ADD_FAILURE() << "failed to delete history db file "
                  << history_path.value();
    return nullptr;
  }

  std::unique_ptr<history::HistoryService> history_service =
      std::make_unique<history::HistoryService>();
  if (history_service->Init(
          history::TestHistoryDatabaseParamsForPath(profile->GetPath()))) {
    return std::move(history_service);
  }

  ADD_FAILURE() << "failed to initialize history service";
  return nullptr;
}

class SearchControllerFake : public SearchController {
 public:
  explicit SearchControllerFake(Profile* profile)
      : SearchController(nullptr, nullptr, nullptr, profile) {}
};

}  // namespace

class SearchResultRankerTest : public testing::Test {
 public:
  SearchResultRankerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SearchResultRankerTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("testuser@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        base::BindRepeating(&BuildHistoryService));

    profile_ = profile_builder.Build();
    Wait();
  }

  void DisableAllFeatures() {
    scoped_feature_list_.InitWithFeaturesAndParameters({}, all_feature_flags_);
  }

  void EnableOneFeature(const base::Feature& feature,
                        const std::map<std::string, std::string>& params = {}) {
    std::vector<base::Feature> disabled;
    for (const auto& f : all_feature_flags_) {
      if (f.name != feature.name)
        disabled.push_back(f);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters({{feature, params}},
                                                       disabled);
  }

  std::unique_ptr<SearchResultRanker> MakeRanker() {
    return std::make_unique<SearchResultRanker>(profile_.get());
  }

  Mixer::SortedResults MakeSearchResults(const std::vector<std::string>& ids,
                                         const std::vector<ResultType>& types,
                                         const std::vector<double> scores) {
    Mixer::SortedResults results;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
      test_search_results_.emplace_back(ids[i], types[i]);
      results.emplace_back(&test_search_results_.back(), scores[i]);
    }
    return results;
  }

  SearchController* MakeSearchController() {
    return new SearchControllerFake(profile_.get());
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  // This is used only to make the ownership clear for the TestSearchResult
  // objects that the return value of MakeSearchResults() contains raw pointers
  // to.
  std::list<TestSearchResult> test_search_results_;

  ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<Profile> profile_;

  const base::HistogramTester histogram_tester_;

 private:
  // All the relevant feature flags for the SearchResultRanker. New experiments
  // should add their flag here.
  std::vector<base::Feature> all_feature_flags_ = {
      app_list_features::kEnableAppRanker,
      app_list_features::kEnableZeroStateMixedTypesRanker};

  DISALLOW_COPY_AND_ASSIGN(SearchResultRankerTest);
};

TEST_F(SearchResultRankerTest, MixedTypesRankersAreDisabledWithFlag) {
  DisableAllFeatures();
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  AppLaunchData app_launch_data;
  app_launch_data.id = "unused";
  app_launch_data.ranking_item_type = RankingItemType::kFile;
  app_launch_data.query = "query";

  for (int i = 0; i < 20; ++i)
    ranker->Train(app_launch_data);
  ranker->FetchRankings(std::u16string());

  auto results =
      MakeSearchResults({"A", "B", "C", "D"},
                        {ResultType::kOmnibox, ResultType::kOmnibox,
                         ResultType::kLauncher, ResultType::kLauncher},
                        {0.6f, 0.5f, 0.4f, 0.3f});

  // Despite training, we expect the scores not to have changed.
  ranker->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("A"), HasId("B"),
                                              HasId("C"), HasId("D"))));
}

TEST_F(SearchResultRankerTest, AppModelImprovesScores) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "frecency",
        "decay_coeff": 0.8
      }
    })";

  EnableOneFeature(app_list_features::kEnableAppRanker,
                   {{"use_recurrence_ranker", "true"}, {"config", json}});
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  AppLaunchData app_A;
  app_A.id = "A";
  app_A.ranking_item_type = RankingItemType::kApp;

  AppLaunchData app_B;
  app_B.id = "B";
  app_B.ranking_item_type = RankingItemType::kApp;

  for (int i = 0; i < 20; ++i) {
    ranker->Train(app_A);
    ranker->Train(app_B);
    ranker->Train(app_A);
  }
  ranker->FetchRankings(std::u16string());

  auto results =
      MakeSearchResults({"A", "B", "C", "D"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kInstalledApp, ResultType::kInstalledApp},
                        {0.1f, 0.2f, 0.3f, 0.4f});

  ranker->Rank(&results);
  // The relevance scores put D > C > B > A, but we've trained on A the most,
  // B half as much, and C and D not at all. So we expect A > B > D > C.
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("A"), HasId("B"),
                                              HasId("D"), HasId("C"))));
}

TEST_F(SearchResultRankerTest, ZeroStateGroupModelDisabledWithFlag) {
  DisableAllFeatures();
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  // TODO(959679): Update the types used in this test once zero-state-related
  // search providers have been implemented.

  AppLaunchData app_launch_data_a;
  app_launch_data_a.id = "A";
  app_launch_data_a.ranking_item_type = RankingItemType::kFile;
  app_launch_data_a.query = "";

  for (int i = 0; i < 10; ++i) {
    ranker->Train(app_launch_data_a);
  }
  ranker->FetchRankings(std::u16string());

  // C and D should be ranked first because their group score should be higher.
  auto results =
      MakeSearchResults({"A", "B", "C", "D"},
                        {ResultType::kLauncher, ResultType::kLauncher,
                         ResultType::kOmnibox, ResultType::kOmnibox},
                        {0.1f, 0.2f, 0.5f, 0.6f});
  ranker->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("D"), HasId("C"),
                                              HasId("B"), HasId("A"))));
}

TEST_F(SearchResultRankerTest, ZeroStateGroupTrainingImprovesScores) {
  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {
                       {"item_coeff", "1.0"},
                       {"group_coeff", "1.0"},
                       {"paired_coeff", "0.0"},
                   });
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  AppLaunchData launch;
  launch.id = "A";
  launch.ranking_item_type = RankingItemType::kZeroStateFile;
  launch.query = "";
  for (int i = 0; i < 10; ++i)
    ranker->Train(launch);
  ranker->FetchRankings(std::u16string());

  // A and B should be ranked first because their group score should be higher.
  auto results =
      MakeSearchResults({"A", "B", "C", "D"},
                        {ResultType::kZeroStateFile, ResultType::kZeroStateFile,
                         ResultType::kOmnibox, ResultType::kOmnibox},
                        {0.1f, 0.2f, 0.1f, 0.2f});
  ranker->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("B"), HasId("A"),
                                              HasId("D"), HasId("C"))));
}

// Without training, the zero state group ranker should produce default scores
// that slightly favour some providers, to break scoring ties. Scores are in
// order Drive > ZeroStateFile > Omnibox. This order should only be changed with
// care.
TEST_F(SearchResultRankerTest, ZeroStateColdStart) {
  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {
                       {"item_coeff", "1.0"},
                       {"group_coeff", "1.0"},
                       {"paired_coeff", "0.0"},
                   });
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  ranker->FetchRankings(std::u16string());
  auto results =
      MakeSearchResults({"Z", "O", "D"},
                        {ResultType::kZeroStateFile, ResultType::kOmnibox,
                         ResultType::kZeroStateDrive},
                        {-0.1f, 0.2f, 0.1f});
  ranker->Rank(&results);

  EXPECT_THAT(results,
              WhenSorted(ElementsAre(HasId("D"), HasId("Z"), HasId("O"))));
}

// If results from all groups are present, the model should not force any
// changes to the ranking.
TEST_F(SearchResultRankerTest, ZeroStateAllGroupsPresent) {
  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {
                       {"item_coeff", "1.0"},
                       {"group_coeff", "0.0"},
                       {"paired_coeff", "0.0"},
                   });
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  auto results = MakeSearchResults(
      {"A2", "O1", "Z1", "Z2", "A1", "D1"},
      {ResultType::kInstalledApp, ResultType::kOmnibox,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kInstalledApp, ResultType::kZeroStateDrive},
      {8.1f, 0.4f, 0.8f, 0.2f, 8.2f, 0.1f});

  ranker->Rank(&results);
  ranker->OverrideZeroStateResults(&results);
  EXPECT_THAT(results,
              WhenSorted(ElementsAre(HasId("A1"), HasId("A2"), HasId("Z1"),
                                     HasId("O1"), HasId("D1"), HasId("Z2"))));
}

// If one group won't have shown results, but has a high-scoring new result
// in the list, it should replace the bottom-most shown result.
TEST_F(SearchResultRankerTest, ZeroStateMissingGroupAdded) {
  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {
                       {"item_coeff", "1.0"},
                       {"group_coeff", "10.0"},
                       {"paired_coeff", "0.0"},
                   });
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  // Train on files enough that they should dominate the zero state results.
  AppLaunchData launch;
  launch.id = "A";
  launch.ranking_item_type = RankingItemType::kZeroStateFile;
  launch.query = "";
  for (int i = 0; i < 10; ++i)
    ranker->Train(launch);
  ranker->FetchRankings(std::u16string());

  auto results = MakeSearchResults(
      {"A1", "A2", "Z1", "Z2", "Z3", "Z4", "Z5", "D1", "D2"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kZeroStateFile, ResultType::kZeroStateDrive,
       ResultType::kZeroStateDrive},
      {8.2f, 8.1f, 1.0f, 0.95f, 0.9f, 0.85f, 0.8f, 0.3f, 0.7f});

  ranker->Rank(&results);
  ranker->OverrideZeroStateResults(&results);
  // Z3 and D1 should be swapped.
  EXPECT_THAT(results,
              WhenSorted(ElementsAre(HasId("A1"), HasId("A2"), HasId("Z1"),
                                     HasId("Z2"), HasId("Z3"), HasId("Z4"),
                                     HasId("D2"), HasId("Z5"), HasId("D1"))));
}

// If two group won't have shown results but meet the conditions for inclusion,
// both should have a result in the list.
TEST_F(SearchResultRankerTest, ZeroStateTwoMissingGroupsAdded) {
  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {
                       {"item_coeff", "1.0"},
                       {"group_coeff", "10.0"},
                       {"paired_coeff", "0.0"},
                   });
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  // Train on files enough that they should dominate the zero state results.
  AppLaunchData launch;
  launch.id = "A";
  launch.ranking_item_type = RankingItemType::kZeroStateFile;
  launch.query = "";
  for (int i = 0; i < 10; ++i)
    ranker->Train(launch);
  ranker->FetchRankings(std::u16string());

  auto results =
      MakeSearchResults({"Z1", "Z2", "Z3", "Z4", "Z5", "D1", "O1"},
                        {ResultType::kZeroStateFile, ResultType::kZeroStateFile,
                         ResultType::kZeroStateFile, ResultType::kZeroStateFile,
                         ResultType::kZeroStateFile,
                         ResultType::kZeroStateDrive, ResultType::kOmnibox},
                        {1.0f, 0.95f, 0.9f, 0.85f, 0.8f, 0.75f, 0.7f});

  ranker->Rank(&results);
  ranker->OverrideZeroStateResults(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(
                           HasId("Z1"), HasId("Z2"), HasId("Z3"), HasId("D1"),
                           HasId("O1"), HasId("Z4"), HasId("Z5"))));
}

// If one group won't have shown results and has a new result in the list, but
// that result has recently been shown twice, it shouldn't be shown.
TEST_F(SearchResultRankerTest, ZeroStateStaleResultIgnored) {
  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {
                       {"item_coeff", "1.0"},
                       {"group_coeff", "10.0"},
                       {"paired_coeff", "0.0"},
                   });
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  // Train on files enough that they should dominate the zero state results.
  AppLaunchData launch;
  launch.id = "A";
  launch.ranking_item_type = RankingItemType::kZeroStateFile;
  launch.query = "";
  for (int i = 0; i < 10; ++i)
    ranker->Train(launch);
  ranker->FetchRankings(std::u16string());

  const auto results = MakeSearchResults(
      {"A1", "A2", "Z1", "Z2", "Z3", "Z4", "Z5", "D1"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kZeroStateFile, ResultType::kZeroStateDrive},
      {8.2f, 8.1f, 1.0f, 0.95f, 0.9f, 0.85f, 0.8f, 0.7f});

  for (int i = 0; i < 3; ++i) {
    auto results_copy = results;
    ranker->Rank(&results_copy);
    ranker->OverrideZeroStateResults(&results_copy);
    // Z3 and D1 should be swapped.
    EXPECT_THAT(results_copy,
                WhenSorted(ElementsAre(HasId("A1"), HasId("A2"), HasId("Z1"),
                                       HasId("Z2"), HasId("Z3"), HasId("Z4"),
                                       HasId("D1"), HasId("Z5"))));
    // D1 should increment its cache counter.
    ranker->ZeroStateResultsDisplayed({{"D1", 0.0f}});
  }

  auto results_copy = results;
  ranker->Rank(&results_copy);
  ranker->OverrideZeroStateResults(&results_copy);
  // Z3 and D1 should NOT be swapped because D1's cache count is too high.
  EXPECT_THAT(results_copy,
              WhenSorted(ElementsAre(HasId("A1"), HasId("A2"), HasId("Z1"),
                                     HasId("Z2"), HasId("Z3"), HasId("Z4"),
                                     HasId("Z5"), HasId("D1"))));
}

// If a group's top result changes, its cache count should reset.
TEST_F(SearchResultRankerTest, ZeroStateCacheResetWhenTopResultChanges) {
  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {
                       {"item_coeff", "1.0"},
                       {"group_coeff", "10.0"},
                       {"paired_coeff", "0.0"},
                   });
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  // Train on files enough that they should dominate the zero state results.
  AppLaunchData launch;
  launch.id = "A";
  launch.ranking_item_type = RankingItemType::kZeroStateFile;
  launch.query = "";
  for (int i = 0; i < 10; ++i)
    ranker->Train(launch);
  ranker->FetchRankings(std::u16string());

  const auto results_1 = MakeSearchResults(
      {"A1", "A2", "Z1", "Z2", "Z3", "Z4", "Z5", "D1", "D2"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kZeroStateFile, ResultType::kZeroStateDrive,
       ResultType::kZeroStateDrive},
      {8.2f, 8.1f, 1.0f, 0.95f, 0.9f, 0.85f, 0.8f, 0.7f, 0.1f});
  const auto results_2 = MakeSearchResults(
      {"A1", "A2", "Z1", "Z2", "Z3", "Z4", "Z5", "D2", "D1"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kZeroStateFile, ResultType::kZeroStateFile,
       ResultType::kZeroStateFile, ResultType::kZeroStateDrive,
       ResultType::kZeroStateDrive},
      {8.2f, 8.1f, 1.0f, 0.95f, 0.9f, 0.85f, 0.8f, 0.7f, 0.1f});

  for (int i = 0; i < 3; ++i) {
    auto results_copy = results_1;
    ranker->Rank(&results_copy);
    ranker->OverrideZeroStateResults(&results_copy);
    // Z3 and D1 should be swapped.
    EXPECT_THAT(results_copy,
                WhenSorted(ElementsAre(HasId("A1"), HasId("A2"), HasId("Z1"),
                                       HasId("Z2"), HasId("Z3"), HasId("Z4"),
                                       HasId("D1"), HasId("Z5"), HasId("D2"))));
    // D1 should increment its cache counter.
    ranker->ZeroStateResultsDisplayed({{"D1", 0.0f}});
  }

  {
    auto results_copy = results_1;
    ranker->Rank(&results_copy);
    ranker->OverrideZeroStateResults(&results_copy);
    // Z3 and D1 should NOT be swapped because D1's cache count is too high.
    EXPECT_THAT(results_copy,
                WhenSorted(ElementsAre(HasId("A1"), HasId("A2"), HasId("Z1"),
                                       HasId("Z2"), HasId("Z3"), HasId("Z4"),
                                       HasId("Z5"), HasId("D1"), HasId("D2"))));
  }

  {
    auto results_copy = results_2;
    ranker->Rank(&results_copy);
    ranker->OverrideZeroStateResults(&results_copy);
    // D2 should override Z3 because the Drive cache count is reset.
    EXPECT_THAT(results_copy,
                WhenSorted(ElementsAre(HasId("A1"), HasId("A2"), HasId("Z1"),
                                       HasId("Z2"), HasId("Z3"), HasId("Z4"),
                                       HasId("D2"), HasId("Z5"), HasId("D1"))));
  }
}

TEST_F(SearchResultRankerTest, ZeroStateGroupRankerUsesFinchConfig) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "fake"
      }
    })";

  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {{"config", json}});

  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  // We expect a FakePredictor to have been loaded because predictor_type is set
  // to "fake" in the json config.
  ASSERT_THAT(ranker->zero_state_group_ranker_->GetPredictorNameForTesting(),
              StrEq(FakePredictor::kPredictorName));
}

// The zero state result type should be logged whenever a zero state item is
// clicked and trained on.
TEST_F(SearchResultRankerTest, ZeroStateClickedTypeMetrics) {
  EnableOneFeature(app_list_features::kEnableZeroStateMixedTypesRanker,
                   {
                       {"item_coeff", "1.0"},
                       {"group_coeff", "1.0"},
                       {"paired_coeff", "0.0"},
                       {"default_group_score", "0.1"},
                   });
  auto ranker = MakeRanker();
  ranker->InitializeRankers(MakeSearchController());
  Wait();

  // Zero state types should be logged during training.

  AppLaunchData app_launch_data_a;
  app_launch_data_a.id = "A";
  app_launch_data_a.ranking_item_type = RankingItemType::kFile;
  app_launch_data_a.query = "";

  ranker->Train(app_launch_data_a);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResults.LaunchedItemType",
      ZeroStateResultType::kUnanticipated, 1);

  AppLaunchData app_launch_data_b;
  app_launch_data_b.id = "B";
  app_launch_data_b.ranking_item_type = RankingItemType::kOmniboxGeneric;
  app_launch_data_b.query = "";

  ranker->Train(app_launch_data_b);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResults.LaunchedItemType",
      ZeroStateResultType::kOmniboxSearch, 1);

  AppLaunchData app_launch_data_c;
  app_launch_data_c.id = "D";
  app_launch_data_c.ranking_item_type = RankingItemType::kDriveQuickAccess;
  app_launch_data_c.query = "";

  ranker->Train(app_launch_data_c);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResults.LaunchedItemType",
      ZeroStateResultType::kDriveQuickAccess, 1);
}

}  // namespace app_list
