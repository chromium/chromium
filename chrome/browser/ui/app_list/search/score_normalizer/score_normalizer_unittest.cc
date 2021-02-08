// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/score_normalizer/score_normalizer.h"

#include <cmath>
#include <numeric>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/score_normalizer/balanced_reservoir.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoubleNear;
using testing::ElementsAre;
using testing::Pointwise;

namespace app_list {

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult() {}
  ~TestSearchResult() override {}
  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

class ScoreNormalizerTest : public testing::Test {
 public:
  using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("Profile 1");
    normalizer_ = std::make_unique<ScoreNormalizer>("apps", profile_,
                                                    /*reservoir_size=*/5);
  }

  void TearDown() override {
    profile_manager_->DeleteTestingProfile("Profile 1");
  }

  ScoreNormalizer::Results MakeSearchResults(
      const std::vector<double>& scores) {
    ScoreNormalizer::Results results;
    for (int i = 0; i < scores.size(); ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_relevance(scores[i]);
      results.push_back(std::move(result));
    }
    return results;
  }

  std::vector<double> ConvertResultsToScores(const Results& results) const {
    std::vector<double> scores;
    for (const auto& result : results) {
      scores.push_back(result->relevance());
    }
    return scores;
  }

 protected:
  std::vector<double> get_dividers() {
    return normalizer_->reservoir_.get_dividers();
  }

  std::vector<double> get_counts() {
    return normalizer_->reservoir_.get_counts();
  }

  std::unique_ptr<ScoreNormalizer> normalizer_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  Profile* profile_;
  content::BrowserTaskEnvironment task_environment_;
};

// Check the conversion of vector of ChromeSearchResult's to scores.
TEST_F(ScoreNormalizerTest, DefaultConvertResultsToScores) {
  ScoreNormalizer::Results results =
      ScoreNormalizerTest::MakeSearchResults({0.9, 1.0, 1.2, 1.5});
  std::vector<double> scores = ConvertResultsToScores(results);
  EXPECT_THAT(scores, ElementsAre(0.9, 1.0, 1.2, 1.5));
}

// Check when normalizing a score and no data is present we return 1.
TEST_F(ScoreNormalizerTest, NormalizeScoreEmpty) {
  EXPECT_EQ(normalizer_->NormalizeScore(-100), 1);
}

// Check RecordScore() and NormalizeScore() functions.
TEST_F(ScoreNormalizerTest, RecordNormalizeScore) {
  normalizer_->RecordScore(1);
  EXPECT_THAT(get_dividers(), ElementsAre(1));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0, 0, 0, 0, 0));
  EXPECT_EQ(normalizer_->NormalizeScore(10), 1.9 / 2);
  EXPECT_EQ(normalizer_->NormalizeScore(-10), 1 / 24.0);

  normalizer_->RecordScore(1);
  EXPECT_THAT(get_dividers(), ElementsAre(1, 1));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0, 0, 0, 0, 0));
  EXPECT_EQ(normalizer_->NormalizeScore(1), 2.0 / 3);
  EXPECT_EQ(normalizer_->NormalizeScore(0.5), 1 / 4.5);

  normalizer_->RecordScore(3);
  EXPECT_THAT(get_dividers(), ElementsAre(1, 1, 3));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0, 0, 0, 0, 0));
  EXPECT_EQ(normalizer_->NormalizeScore(1), 2.0 / 4);
  EXPECT_EQ(normalizer_->NormalizeScore(-1), 1 / 12.0);
  EXPECT_EQ(normalizer_->NormalizeScore(10), (3 + 7.0 / 8) / 4);

  normalizer_->RecordScore(-1);
  EXPECT_THAT(get_dividers(), ElementsAre(-1, 1, 1, 3));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0, 0, 0, 0, 0));
  EXPECT_EQ(normalizer_->NormalizeScore(1), 3.0 / 5);
  EXPECT_EQ(normalizer_->NormalizeScore(-1), 1.0 / 5);
  EXPECT_EQ(normalizer_->NormalizeScore(10), (4 + 7.0 / 8) / 5);
  EXPECT_EQ(normalizer_->NormalizeScore(-10), 0.1 / 5);

  normalizer_->RecordScore(5);
  EXPECT_THAT(get_dividers(), ElementsAre(-1, 1, 1, 3, 5));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0, 0, 0, 0, 0));
  EXPECT_EQ(normalizer_->NormalizeScore(1), 3.0 / 6);
  EXPECT_EQ(normalizer_->NormalizeScore(-1), 1.0 / 6);
  EXPECT_EQ(normalizer_->NormalizeScore(10), (5 + 5.0 / 6) / 6);
  EXPECT_EQ(normalizer_->NormalizeScore(-10), 0.1 / 6);
  EXPECT_EQ(normalizer_->NormalizeScore(3), 4.0 / 6);

  normalizer_->RecordScore(1);
  EXPECT_THAT(get_dividers(), ElementsAre(1, 1, 1, 3, 5));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0, 0.5, 0.5, 0, 0));

  normalizer_->RecordScore(-1);
  EXPECT_THAT(get_dividers(), ElementsAre(-1, 1, 1, 1, 3));
  EXPECT_THAT(get_counts(), ElementsAre(0.5, 0.5, 0, 0.5, 0.5, 0));

  normalizer_->RecordScore(7);
  EXPECT_THAT(get_dividers(), ElementsAre(-1, 1, 1, 3, 7));
  EXPECT_THAT(get_counts(), ElementsAre(0.5, 0.5, 0.5, 0.5, 0.5, 0.5));
}

// Check record results for results of size < reservoir size.
TEST_F(ScoreNormalizerTest, SmallRecordResults) {
  ScoreNormalizer::Results results =
      ScoreNormalizerTest::MakeSearchResults({1.0});
  std::vector<double> scores = ConvertResultsToScores(results);
  normalizer_->RecordResults(results);
  EXPECT_THAT(get_dividers(), ElementsAre(1.0));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0, 0, 0, 0, 0));
}

// Check record results for results of size = reservoir size.
TEST_F(ScoreNormalizerTest, MediumRecordResults) {
  ScoreNormalizer::Results results =
      ScoreNormalizerTest::MakeSearchResults({3.0, 2.0, 3.0, -1.0, 10.0});
  std::vector<double> scores = ConvertResultsToScores(results);
  normalizer_->RecordResults(results);
  EXPECT_THAT(get_dividers(), ElementsAre(-1.0, 2.0, 3.0, 3.0, 10.0));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0, 0, 0, 0, 0));
}

// Check record results for results of size > reservoir size.
TEST_F(ScoreNormalizerTest, LargeRecordResults) {
  ScoreNormalizer::Results results = ScoreNormalizerTest::MakeSearchResults(
      {3.0, 2.0, 3.0, -1.0, 10.0, 5.0, 7.0, 20.0, -2.0});
  std::vector<double> scores = ConvertResultsToScores(results);
  normalizer_->RecordResults(results);
  EXPECT_THAT(get_dividers(), ElementsAre(3.0, 5.0, 7.0, 10.0, 20.0));
  EXPECT_THAT(get_counts(), ElementsAre(1, 0.5, 0.75, 0.75, 0.5, 0.5));
}

// Check normalizing of results for results of size < reservoir size.
TEST_F(ScoreNormalizerTest, SmallNormalizeScores) {
  ScoreNormalizer::Results results =
      ScoreNormalizerTest::MakeSearchResults({0.9, 1.0, 1.2, 1.5});
  normalizer_->RecordResults(results);
  normalizer_->NormalizeResults(&results);
  EXPECT_THAT(get_dividers(), ElementsAre(0.9, 1.0, 1.2, 1.5));
  EXPECT_THAT(ConvertResultsToScores(results),
              Pointwise(DoubleNear(1e-10), {0.2, 0.4, 0.6, 0.8}));
}

// Check normalizing of results for results of size = reservoir size.
TEST_F(ScoreNormalizerTest, MediumNormalizeScores) {
  ScoreNormalizer::Results results =
      ScoreNormalizerTest::MakeSearchResults({0.9, 1.0, 1.2, 1.5, -0.1, 0.2});
  normalizer_->RecordResults(results);
  normalizer_->NormalizeResults(&results);
  EXPECT_THAT(get_dividers(), ElementsAre(-0.1, 0.2, 0.9, 1.2, 1.5));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0.5, 0.5, 0, 0, 0));
  EXPECT_THAT(ConvertResultsToScores(results),
              Pointwise(DoubleNear(1e-10), {3.0 / 6, (3 + 1.0 / 3) / 6, 4.0 / 6,
                                            5.0 / 6, 1.0 / 6, 2.0 / 6}));
}

// Check normalizing of results for results of size > reservoir size.
TEST_F(ScoreNormalizerTest, LargeNormalizeResults) {
  ScoreNormalizer::Results results = ScoreNormalizerTest::MakeSearchResults(
      {0.9, 1.0, 1.2, 1.5, -0.1, 0.2, 0.5, 1.1});
  normalizer_->RecordResults(results);
  normalizer_->NormalizeResults(&results);
  EXPECT_THAT(get_dividers(), ElementsAre(0.2, 0.5, 0.9, 1.1, 1.5));
  EXPECT_THAT(get_counts(), ElementsAre(0.5, 0.75, 0.75, 0.5, 0.5, 0));
  EXPECT_THAT(
      ConvertResultsToScores(results),
      Pointwise(DoubleNear(1e-10), {3.0 / 6, 3.5 / 6, 4.25 / 6, 5.0 / 6,
                                    1 / (1.3 * 6), 1.0 / 6, 2.0 / 6, 4.0 / 6}));
}

// Check record and normalizing of multiple results.
TEST_F(ScoreNormalizerTest, MultipleRecordAndNormalizeResults) {
  ScoreNormalizer::Results results1 =
      ScoreNormalizerTest::MakeSearchResults({0.9, 1.0, 1.2, 1.5, -0.1, 0.2});
  ScoreNormalizer::Results results2 =
      ScoreNormalizerTest::MakeSearchResults({0.5, 1.1});
  normalizer_->RecordResults(results1);
  normalizer_->NormalizeResults(&results1);
  EXPECT_THAT(get_dividers(), ElementsAre(-0.1, 0.2, 0.9, 1.2, 1.5));
  EXPECT_THAT(get_counts(), ElementsAre(0, 0.5, 0.5, 0, 0, 0));
  normalizer_->RecordResults(results2);
  normalizer_->NormalizeResults(&results2);
  EXPECT_THAT(get_dividers(), ElementsAre(0.2, 0.5, 0.9, 1.1, 1.5));
  EXPECT_THAT(get_counts(), ElementsAre(0.5, 0.75, 0.75, 0.5, 0.5, 0));
  EXPECT_THAT(ConvertResultsToScores(results1),
              Pointwise(DoubleNear(1e-10), {3.0 / 6, (3 + 1.0 / 3) / 6, 4.0 / 6,
                                            5.0 / 6, 1.0 / 6, 2.0 / 6}));
  EXPECT_THAT(ConvertResultsToScores(results2),
              Pointwise(DoubleNear(1e-10), {2.0 / 6, 4.0 / 6}));
}

}  // namespace app_list