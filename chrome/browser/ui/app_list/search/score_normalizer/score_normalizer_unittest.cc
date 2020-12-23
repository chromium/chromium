// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/score_normalizer/score_normalizer.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoubleNear;
using testing::UnorderedElementsAre;
using testing::UnorderedPointwise;

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
    normalizer_ = std::make_unique<ScoreNormalizer>("apps", profile_);
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

 protected:
  std::vector<double> ConvertResultsToScores(
      const ScoreNormalizer::Results& results) {
    return normalizer_->ConvertResultsToScores(results);
  }

  void UpdateDistribution(const std::vector<double>& scores) {
    normalizer_->UpdateDistribution(scores);
  }

  double get_mean() { return normalizer_->mean_; }

  int get_num_results() { return normalizer_->num_results_; }

  // For testing integer overflow.
  void set_num_results(int num) { normalizer_->num_results_ = num; }

  std::unique_ptr<ScoreNormalizer> normalizer_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  Profile* profile_;
  content::BrowserTaskEnvironment task_environment_;
};

// Check that get_provider returns the correct provider.
TEST_F(ScoreNormalizerTest, DefaultGetProvider) {
  EXPECT_EQ("apps", normalizer_->get_provider());
}

// Check the conversion of vector of ChromeSearchResult's to scores.
TEST_F(ScoreNormalizerTest, DefaultConvertResultsToScores) {
  ScoreNormalizer::Results results =
      ScoreNormalizerTest::MakeSearchResults({0.9, 1.0, 1.2, 1.5});
  std::vector<double> scores = ConvertResultsToScores(results);
  EXPECT_THAT(scores, UnorderedElementsAre(0.9, 1.0, 1.2, 1.5));
}

// Check if no results are recorded this is empty.
TEST_F(ScoreNormalizerTest, EmptyConvertResultsToScores) {
  ScoreNormalizer::Results results = ScoreNormalizerTest::MakeSearchResults({});
  std::vector<double> scores = ConvertResultsToScores(results);
  EXPECT_TRUE(scores.empty());
}

// Check that updating the distribution
// changes the mean and the number of results correctly.
TEST_F(ScoreNormalizerTest, DefaultUpdateDistribution) {
  std::vector<double> scores = {-1, 2, 4, 1};
  UpdateDistribution(scores);
  EXPECT_DOUBLE_EQ(get_mean(), 1.5);
  EXPECT_EQ(get_num_results(), 4);
}

// Check that updating the distribution multiple times
// changes the mean and the number of results correctly.
TEST_F(ScoreNormalizerTest, MultipleUpdateDistribution) {
  std::vector<double> scores1 = {-1, 2, 4, 1};
  std::vector<double> scores2 = {0, 0};
  std::vector<double> scores3 = {1.1, 2.2, 3.3, 4.4};
  UpdateDistribution(scores1);
  UpdateDistribution(scores2);
  UpdateDistribution(scores3);
  EXPECT_DOUBLE_EQ(get_mean(), 1.7);
  EXPECT_EQ(get_num_results(), 10);
}

// Update distribution with no results.
// This should not change the mean or number of results.
TEST_F(ScoreNormalizerTest, EmptyUpdateDistribution) {
  std::vector<double> scores = {};
  UpdateDistribution(scores);
  EXPECT_DOUBLE_EQ(get_mean(), 0);
  EXPECT_EQ(get_num_results(), 0);
}

// Check that if there is a lot of results, that is enough to cause int
// overflow. Addition of any further results will not update
// the distribution.
TEST_F(ScoreNormalizerTest, OverflowUpdateDistribution) {
  // set_num_results() is equivalent to adding INT_MAX-2 scores
  // which are all equal to 0.
  set_num_results(INT_MAX - 2);
  // Updating the results with scores1 will cause int overflow
  // of num_results_. So updating the distribution with scores1
  // will not actually update the distribution.
  std::vector<double> scores1 = {1, 1, 1, 1};
  UpdateDistribution(scores1);
  // Updating the results with scores2 will not cause int overflow
  // of num_results_. So this will update the distribution.
  std::vector<double> scores2 = {1, 1};
  UpdateDistribution(scores2);
  EXPECT_DOUBLE_EQ(get_mean(), 2.0 / INT_MAX);
  EXPECT_EQ(get_num_results(), INT_MAX);
}

// Check scores are recorded and normalized with the mean.
TEST_F(ScoreNormalizerTest, DefaultRecordNormalize) {
  ScoreNormalizer::Results results =
      ScoreNormalizerTest::MakeSearchResults({0.9, 1.0, 1.2, 1.5});
  normalizer_->Record(results);
  double new_score = normalizer_->NormalizeScore(1.5);
  normalizer_->NormalizeResults(&results);
  std::vector<double> normalized_results{-0.25, -0.15, 0.05, 0.35};
  EXPECT_DOUBLE_EQ(new_score, 0.35);
  EXPECT_DOUBLE_EQ(get_mean(), 1.15);
  EXPECT_EQ(get_num_results(), 4);
  EXPECT_THAT(ConvertResultsToScores(results),
              UnorderedPointwise(DoubleNear(1e-10), normalized_results));
}

// Check multiple score recordings.
TEST_F(ScoreNormalizerTest, MultipleRecordNormalize) {
  ScoreNormalizer::Results results1 =
      ScoreNormalizerTest::MakeSearchResults({0, 1, 2, 3, 4, 5});
  ScoreNormalizer::Results results2 =
      ScoreNormalizerTest::MakeSearchResults({0.9, 1.0, 1.2, 1.5});
  ScoreNormalizer::Results results3 =
      ScoreNormalizerTest::MakeSearchResults({0, 1.1, 2.2, 3.3, 4.4});
  normalizer_->Record(results1);
  normalizer_->Record(results2);
  normalizer_->Record(results3);
  normalizer_->NormalizeResults(&results1);
  normalizer_->NormalizeResults(&results2);
  normalizer_->NormalizeResults(&results3);
  std::vector<double> normalized_results1{-2.04, -1.04, -0.04,
                                          0.96,  1.96,  2.96};
  std::vector<double> normalized_results2{-1.14, -1.04, -0.84, -0.54};
  std::vector<double> normalized_results3{-2.04, -0.94, 0.16, 1.26, 2.36};
  double new_score = normalizer_->NormalizeScore(1.5);
  EXPECT_DOUBLE_EQ(new_score, -0.54);
  EXPECT_DOUBLE_EQ(get_mean(), 2.04);
  EXPECT_EQ(get_num_results(), 15);
  EXPECT_THAT(ConvertResultsToScores(results1),
              UnorderedPointwise(DoubleNear(1e-10), normalized_results1));
  EXPECT_THAT(ConvertResultsToScores(results2),
              UnorderedPointwise(DoubleNear(1e-10), normalized_results2));
  EXPECT_THAT(ConvertResultsToScores(results3),
              UnorderedPointwise(DoubleNear(1e-10), normalized_results3));
}

// Check that if you record no results.
// When you normalize it should return the original value.
TEST_F(ScoreNormalizerTest, EmptyRecordNormalize) {
  ScoreNormalizer::Results results = ScoreNormalizerTest::MakeSearchResults({});
  normalizer_->Record(results);
  double new_score = normalizer_->NormalizeScore(1.5);
  normalizer_->NormalizeResults(&results);
  EXPECT_DOUBLE_EQ(new_score, 1.5);
  EXPECT_DOUBLE_EQ(get_mean(), 0);
  EXPECT_EQ(get_num_results(), 0);
  EXPECT_TRUE(results.empty());
}

}  // namespace app_list