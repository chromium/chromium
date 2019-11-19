// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"

#include <tuple>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class HistogramUtilTest : public testing::Test {
 public:
  HistogramUtilTest() {}
  ~HistogramUtilTest() override {}

  const base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistogramUtilTest);
};

TEST_F(HistogramUtilTest, TestLaunchedItemPosition) {
  std::vector<RankingItemType> result_types = {
      RankingItemType::kOmniboxGeneric, RankingItemType::kZeroStateFile,
      RankingItemType::kZeroStateFile, RankingItemType::kZeroStateFile};

  // Don't log if there is no click.
  LogZeroStateResultsListMetrics(result_types, -1);
  histogram_tester_.ExpectTotalCount(
      "Apps.AppList.ZeroStateResultsList.LaunchedItemPositionV2", 0);

  // Log some actual clicks.
  LogZeroStateResultsListMetrics(result_types, 3);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.LaunchedItemPositionV2", 3, 1);

  LogZeroStateResultsListMetrics(result_types, 2);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.LaunchedItemPositionV2", 2, 1);
}

TEST_F(HistogramUtilTest, TestNumImpressionTypes) {
  // No results.
  std::vector<RankingItemType> result_types_1;
  LogZeroStateResultsListMetrics(result_types_1, 0);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.NumImpressionTypesV2", 0, 1);

  // Several types of results.
  std::vector<RankingItemType> result_types_2 = {
      RankingItemType::kOmniboxGeneric, RankingItemType::kZeroStateFile,
      RankingItemType::kDriveQuickAccess};
  LogZeroStateResultsListMetrics(result_types_2, 0);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.NumImpressionTypesV2", 3, 1);

  // Some types doubled up.
  std::vector<RankingItemType> result_types_3 = {
      RankingItemType::kOmniboxGeneric, RankingItemType::kZeroStateFile,
      RankingItemType::kZeroStateFile, RankingItemType::kZeroStateFile};
  LogZeroStateResultsListMetrics(result_types_3, 0);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.NumImpressionTypesV2", 2, 1);
}

TEST_F(HistogramUtilTest, TestContainsDriveFiles) {
  // No Drive files
  LogZeroStateResultsListMetrics(
      {RankingItemType::kOmniboxGeneric, RankingItemType::kZeroStateFile}, 0);
  histogram_tester_.ExpectUniqueSample(
      "Apps.AppList.ZeroStateResultsList.ContainsDriveFiles", 0, 1);

  // One Drive file
  LogZeroStateResultsListMetrics(
      {RankingItemType::kOmniboxGeneric, RankingItemType::kDriveQuickAccess,
       RankingItemType::kZeroStateFile},
      0);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.ContainsDriveFiles", 1, 1);

  // Many Drive files
  LogZeroStateResultsListMetrics(
      {RankingItemType::kDriveQuickAccess, RankingItemType::kDriveQuickAccess,
       RankingItemType::kDriveQuickAccess},
      0);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.ContainsDriveFiles", 1, 2);
}

class HistogramUtilScoreTest
    : public HistogramUtilTest,
      public testing::WithParamInterface<std::tuple<float, float, float, int>> {
  void SetUp() override {}
};

TEST_P(HistogramUtilScoreTest, ScoreTest) {
  LogZeroStateReceivedScore("Test", std::get<0>(GetParam()),
                            std::get<1>(GetParam()), std::get<2>(GetParam()));
  histogram_tester_.ExpectUniqueSample(
      "Apps.AppList.ZeroStateResults.ReceivedScore.Test",
      std::get<3>(GetParam()), 1);
}

constexpr std::tuple<float, float, float, int> kTestValues[] = {
    {0.0f, 0.0f, 1.0f, 0},       {0.5f, 0.0f, 1.0f, 50},
    {1.0f, 0.0f, 1.0f, 100},     {-10.0f, -10.0f, 10.0f, 0},
    {-5.0f, -10.0f, 10.0f, 25},  {0.0f, -10.0f, 10.0f, 50},
    {5.0f, -10.0f, 10.0f, 75},   {10.0f, -10.0f, 10.0f, 100},
    {-100.0f, -10.0f, 10.0f, 0}, {150000.0f, -10.0f, 10.0f, 100},
};

INSTANTIATE_TEST_SUITE_P(HistogramUtilScoreTest,
                         HistogramUtilScoreTest,
                         testing::ValuesIn(kTestValues));

}  // namespace app_list
