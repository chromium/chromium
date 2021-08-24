// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/score_normalizer/balanced_reservoir.h"

#include <cfloat>
#include <cmath>
#include <numeric>
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

using testing::ElementsAre;

namespace app_list {

class BalancedReservoirTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("Profile 1");
    reservoir_ =
        std::make_unique<BalancedReservoir>("apps", profile_, /*size=*/5);
  }

  void TearDown() override {
    profile_manager_->DeleteTestingProfile("Profile 1");
  }

 protected:
  std::unique_ptr<BalancedReservoir> reservoir_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  Profile* profile_;
  content::BrowserTaskEnvironment task_environment_;
};

double get_error(std::vector<double> counts) {
  const double mean = std::accumulate(counts.begin(), counts.end(), 0.0) /
                      static_cast<double>(counts.size());
  double error = 0;
  for (const double count : counts) {
    error = error + std::pow((count - mean), 2);
  }
  return error / static_cast<double>(counts.size());
}

// Check the GetBin() function for a standard vector of dividers.
TEST_F(BalancedReservoirTest, GetBinDefault) {
  reservoir_->SetStateForTest({-1.0, 2.0, 3.0, 3.0, 10.0}, {0, 0, 0, 0, 0, 0});
  EXPECT_THAT(reservoir_->GetBin(2.0), 2);
  EXPECT_THAT(reservoir_->GetBin(-100.0), 0);
  EXPECT_THAT(reservoir_->GetBin(3.0), 4);
  EXPECT_THAT(reservoir_->GetBin(11.0), 5);
}

// Check the GetBin() function for a vector of identical dividers.
TEST_F(BalancedReservoirTest, GetBinIdenticalScores) {
  reservoir_->SetStateForTest({0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});
  EXPECT_THAT(reservoir_->GetBin(0), 5);
}

// Check the GetBin() function for empty vector of dividers.
TEST_F(BalancedReservoirTest, GetBinEmptyScores) {
  EXPECT_TRUE(reservoir_->get_dividers().empty());
  EXPECT_THAT(reservoir_->GetBin(0), 0);
}

// Check the RecordScore() function for small dividers.
TEST_F(BalancedReservoirTest, RecordScoreSmall) {
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(0));
  reservoir_->RecordScore(-1.0);
  EXPECT_THAT(reservoir_->get_dividers(), ElementsAre(-1.0));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(0, 0));
  reservoir_->RecordScore(10);
  EXPECT_THAT(reservoir_->get_dividers(), ElementsAre(-1.0, 10));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(0, 0, 0));
  reservoir_->RecordScore(2);
  EXPECT_THAT(reservoir_->get_dividers(), ElementsAre(-1.0, 2, 10));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(0, 0, 0, 0));
  reservoir_->RecordScore(2);
  EXPECT_THAT(reservoir_->get_dividers(), ElementsAre(-1.0, 2, 2, 10));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(0, 0, 0, 0, 0));
}

// Check the RecordScore() function with splitting and merging.
TEST_F(BalancedReservoirTest, RecordScoreLarge) {
  reservoir_->SetStateForTest({1, 2, 3, 4, 5}, {0, 0, 0, 0, 0, 0});
  reservoir_->RecordScore(2);
  EXPECT_THAT(reservoir_->get_dividers(), ElementsAre(2.0, 2.0, 3.0, 4.0, 5.0));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(0, 0.5, 0.5, 0, 0, 0));
  reservoir_->RecordScore(-2);
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(-2.0, 2.0, 2.0, 3.0, 5.0));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(0.5, 0.5, 0.5, 0.5, 0, 0));
  reservoir_->RecordScore(10);
  EXPECT_THAT(reservoir_->get_dividers(), ElementsAre(-2.0, 2.0, 2.0, 5.0, 10));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(0.5, 0.5, 0.5, 0.5, 0.5, 0.5));
  reservoir_->RecordScore(2);
  EXPECT_THAT(reservoir_->get_dividers(), ElementsAre(2.0, 2.0, 2.0, 5.0, 10));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(1, 0.5, 0.75, 0.75, 0.5, 0.5));
}

// Check when counts approach double overflow all values are halved.
// Counts are then updated normally, bins are then split and merged.
TEST_F(BalancedReservoirTest, CountsDoubleOverflow) {
  reservoir_->SetStateForTest({1, 2, 3, 4, 5}, {DBL_MAX, DBL_MAX, 1, 1, 1, 2});
  reservoir_->RecordScore(0);
  EXPECT_THAT(reservoir_->get_dividers(), ElementsAre(0, 1, 2, 4, 5));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre((DBL_MAX / 2.0 + 1) / 2.0, (DBL_MAX / 2.0 + 1) / 2.0,
                          DBL_MAX / 2.0, 1, 0.5, 1));
}

// Check the SplitBinByScore() function.
TEST_F(BalancedReservoirTest, SplitBinByScore) {
  reservoir_->SetStateForTest({1, 2, 3, 4, 5}, {1, 1, 1, 1, 1, 1});

  reservoir_->SplitBinByScore(reservoir_->GetBin(2.0), 2.0);
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(1.0, 2.0, 2.0, 3.0, 4.0, 5.0));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(1.0, 1.0, 0.5, 0.5, 1.0, 1.0, 1.0));

  reservoir_->SplitBinByScore(reservoir_->GetBin(7.0), 7.0);
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(1.0, 2.0, 2.0, 3.0, 4.0, 5.0, 7.0));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(1.0, 1.0, 0.5, 0.5, 1.0, 1.0, 0.5, 0.5));

  reservoir_->SplitBinByScore(reservoir_->GetBin(-10.0), -10.0);
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(-10.0, 1.0, 2.0, 2.0, 3.0, 4.0, 5.0, 7.0));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(0.5, 0.5, 1.0, 0.5, 0.5, 1.0, 1.0, 0.5, 0.5));
}

// Check MergeSmallestBins() function for standard distribution of counts.
TEST_F(BalancedReservoirTest, MergeSmallestBinsDefault) {
  reservoir_->SetStateForTest({1.0, 2.0, 3.0, 5.0, 5.0, 7.1, 10.2, 11.0},
                              {0.0, 0.0, 1.5, 3.0, 0, 1.0, 5.0, 0.5, 0});
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(1.0, 2.0, 3.0, 5.0, 5.0, 7.1, 10.2, 11.0));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(0, 0, 1.5, 3.0, 0, 1.0, 5.0, 0.5, 0));

  reservoir_->MergeSmallestBins();
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(2.0, 3.0, 5.0, 5.0, 7.1, 10.2, 11.0));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(0, 1.5, 3.0, 0, 1.0, 5.0, 0.5, 0));

  reservoir_->MergeSmallestBins();
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(2.0, 3.0, 5.0, 5.0, 7.1, 10.2));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(0, 1.5, 3.0, 0, 1.0, 5.0, 0.5));

  reservoir_->MergeSmallestBins();
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(2.0, 3.0, 5.0, 7.1, 10.2));
  EXPECT_THAT(reservoir_->get_counts(),
              ElementsAre(0, 1.5, 3.0, 1.0, 5.0, 0.5));
}

// Check MergeSmallestBins() function when all bins are identical.
// It should merge the smallest bin from the left.
TEST_F(BalancedReservoirTest, MergeSmallestBinsIdenticalBins) {
  reservoir_->SetStateForTest({1.0, 2.0, 3.0, 5.0, 5.0, 7.1, 10.2, 11.0},
                              {1, 1, 1, 1, 1, 1, 1, 1, 1});
  reservoir_->MergeSmallestBins();
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(2.0, 3.0, 5.0, 5.0, 7.1, 10.2, 11.0));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(2, 1, 1, 1, 1, 1, 1, 1));

  reservoir_->MergeSmallestBins();
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(2.0, 5.0, 5.0, 7.1, 10.2, 11.0));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(2, 2, 1, 1, 1, 1, 1));

  reservoir_->MergeSmallestBins();
  EXPECT_THAT(reservoir_->get_dividers(),
              ElementsAre(2.0, 5.0, 7.1, 10.2, 11.0));
  EXPECT_THAT(reservoir_->get_counts(), ElementsAre(2, 2, 2, 1, 1, 1));
}

// Check GetError() when all counts are equal.
TEST_F(BalancedReservoirTest, GetErrorEven) {
  reservoir_->SetStateForTest({1, 2, 3, 4, 5}, {2, 2, 2, 2, 2, 2});
  EXPECT_EQ(reservoir_->GetError(), 0);
}

// Check GetError() when counts are unequal.
TEST_F(BalancedReservoirTest, GetErrorUneven) {
  reservoir_->SetStateForTest({1, 2, 3, 4, 5}, {3, 1, 2, 3, 2, 1});
  EXPECT_EQ(reservoir_->GetError(), 2 / 3.0);
}

// Check NormalizeScore() for when no dividers.
TEST_F(BalancedReservoirTest, NormalizeScoreEmpty) {
  EXPECT_EQ(reservoir_->NormalizeScore(100), 0.5);
}

// Check NormalizeScore() for default scores.
TEST_F(BalancedReservoirTest, NormalizeScoreDefault) {
  reservoir_->SetStateForTest({1, 2, 3, 3, 5}, {0, 0, 0, 0, 0, 0});
  EXPECT_EQ(reservoir_->NormalizeScore(0), 0.5 / 6);
  EXPECT_EQ(reservoir_->NormalizeScore(1.5), 1.5 / 6);
  EXPECT_EQ(reservoir_->NormalizeScore(2), 2.0 / 6);
  EXPECT_EQ(reservoir_->NormalizeScore(3), 4.0 / 6);
  EXPECT_EQ(reservoir_->NormalizeScore(7), (5 + 2.0 / 3) / 6);
}

// Check NormalizeScore() for identical scores.
TEST_F(BalancedReservoirTest, NormalizeScoreIdentical) {
  reservoir_->SetStateForTest({1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0});
  EXPECT_EQ(reservoir_->NormalizeScore(1), 5.0 / 6);
  EXPECT_EQ(reservoir_->NormalizeScore(2), 5.5 / 6);
  EXPECT_EQ(reservoir_->NormalizeScore(0), 0.5 / 6);
}

// Check that we can read and write to prefs.
TEST_F(BalancedReservoirTest, ReadAndWritePrefs) {
  {
    auto reservoir =
        std::make_unique<BalancedReservoir>("test", profile_, /*size=*/5);
    EXPECT_TRUE(reservoir->get_dividers().empty());
    EXPECT_THAT(reservoir->get_counts(), ElementsAre(0));
    reservoir->WritePrefs();
  }

  {
    auto reservoir =
        std::make_unique<BalancedReservoir>("test", profile_, /*size=*/5);
    EXPECT_TRUE(reservoir->get_dividers().empty());
    EXPECT_THAT(reservoir->get_counts(), ElementsAre(0));

    reservoir->SetStateForTest({1, 2, 3}, {4, 5, 6, 7});
    reservoir->WritePrefs();
  }

  {
    auto reservoir =
        std::make_unique<BalancedReservoir>("test", profile_, /*size=*/5);
    EXPECT_THAT(reservoir->get_dividers(), ElementsAre(1, 2, 3));
    EXPECT_THAT(reservoir->get_counts(), ElementsAre(4, 5, 6, 7));
  }
}

// Check that existing state in prefs that is incompatible with the
// configuration of the reservoir leads to starting with a blank slate.
//
// The case of interest is when there are more dividers saved in prefs than the
// max_dividers number set at construction.
TEST_F(BalancedReservoirTest, PrefsIgnoredWhenMaxDividersIncompatible) {
  {
    auto reservoir =
        std::make_unique<BalancedReservoir>("test", profile_, /*size=*/5);
    reservoir->SetStateForTest({1, 2, 3, 4, 5}, {6, 7, 8, 9, 10, 11});
    reservoir->WritePrefs();
  }

  {
    auto reservoir =
        std::make_unique<BalancedReservoir>("test", profile_, /*size=*/2);
    EXPECT_TRUE(reservoir->get_dividers().empty());
    EXPECT_THAT(reservoir->get_counts(), ElementsAre(0));
  }
}

}  // namespace app_list
