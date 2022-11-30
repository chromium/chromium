// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace query_tiles {
namespace {

TEST(QueryTilesStatsTest, RecordImageLoading) {
  base::HistogramTester tester;
  stats::RecordImageLoading(stats::ImagePreloadingEvent::kStart);
  tester.ExpectBucketCount(stats::kImagePreloadingHistogram, 0, 1);
}

TEST(QueryTilesStatsTest, RecordTileFetcherResponseCode) {
  base::HistogramTester tester;
  stats::RecordTileFetcherResponseCode(200);
  tester.ExpectBucketCount(stats::kHttpResponseCodeHistogram, 200, 1);
}

TEST(QueryTilesStatsTest, RecordTileFetcherNetErrorCode) {
  base::HistogramTester tester;
  stats::RecordTileFetcherNetErrorCode(105);
  tester.ExpectBucketCount(stats::kNetErrorCodeHistogram, -105, 1);
}

TEST(QueryTilesStatsTest, RecordTileRequestStatus) {
  base::HistogramTester tester;
  stats::RecordTileRequestStatus(TileInfoRequestStatus::kSuccess);
  tester.ExpectBucketCount(stats::kRequestStatusHistogram, 1, 1);
}

TEST(QueryTilesStatsTest, RecordTileGroupStatus) {
  base::HistogramTester tester;
  stats::RecordTileGroupStatus(TileGroupStatus::kNoTiles);
  tester.ExpectBucketCount(stats::kGroupStatusHistogram, 3, 1);
}

TEST(QueryTilesStatsTest, RecordFirstFetchFlowDuration) {
  base::HistogramTester tester;
  stats::RecordFirstFetchFlowDuration(18);
  tester.ExpectBucketCount(stats::kFirstFlowDurationHistogram, 18, 1);
}

TEST(QueryTilesStatsTest, RecordExplodeOnFetchStarted) {
  base::HistogramTester tester;
  stats::RecordExplodeOnFetchStarted(12);
  tester.ExpectBucketCount(stats::kFetcherStartHourHistogram, 12, 1);
}

TEST(QueryTilesStatsTest, RecordGroupPruned) {
  base::HistogramTester tester;
  stats::RecordGroupPruned(stats::PrunedGroupReason::kExpired);
  stats::RecordGroupPruned(stats::PrunedGroupReason::kInvalidLocale);
  tester.ExpectBucketCount(stats::kPrunedGroupReasonHistogram, 0, 1);
  tester.ExpectBucketCount(stats::kPrunedGroupReasonHistogram, 1, 1);
}

TEST(QueryTilesStatsTest, RecordTrendingTileEvent) {
  base::HistogramTester tester;
  stats::RecordTrendingTileEvent(stats::TrendingTileEvent::kRemoved);
  stats::RecordTrendingTileEvent(stats::TrendingTileEvent::kClicked);
  tester.ExpectBucketCount(stats::kTrendingTileEventHistogram, 0, 0);
  tester.ExpectBucketCount(stats::kTrendingTileEventHistogram, 1, 1);
  tester.ExpectBucketCount(stats::kTrendingTileEventHistogram, 2, 1);
}

}  // namespace
}  // namespace query_tiles
