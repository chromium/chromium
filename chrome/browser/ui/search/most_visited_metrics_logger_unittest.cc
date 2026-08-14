// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/most_visited_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::Bucket;
using testing::ElementsAre;

ntp_tiles::NTPTileImpression MakeImpression(int index,
                                            ntp_tiles::TileSource source) {
  return ntp_tiles::NTPTileImpression(
      index, source, ntp_tiles::TileTitleSource::TITLE_TAG,
      ntp_tiles::TileVisualType::ICON_COLOR, favicon_base::IconType::kInvalid,
      /*url_for_rappor=*/GURL());
}

TEST(MostVisitedMetricsLoggerTest, ShouldRecordImpressionsAndLoadState) {
  base::HistogramTester histogram_tester;
  MostVisitedMetricsLogger logger("Omnibox");

  logger.LogMostVisitedImpression(
      MakeImpression(0, ntp_tiles::TileSource::TOP_SITES));
  logger.LogMostVisitedImpression(
      MakeImpression(1, ntp_tiles::TileSource::CUSTOM_LINKS));

  // Impressions shouldn't be recorded before load is logged.
  histogram_tester.ExpectTotalCount("Omnibox.SuggestionsImpression", 0);

  logger.LogMostVisitedLoaded(
      /*time=*/base::Seconds(1), /*using_most_visited=*/false,
      /*using_custom_links=*/true, /*using_enterprise_shortcuts=*/false,
      /*is_visible=*/true, /*is_expanded=*/true);

  EXPECT_THAT(histogram_tester.GetAllSamples("Omnibox.SuggestionsImpression"),
              ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("Omnibox.NumberOfTiles"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Omnibox.MostVisited.IsExpandedOnLoad"),
      ElementsAre(Bucket(1, 1)));

  // Secondary calls to LogMostVisitedLoaded should not re-flush impressions.
  logger.LogMostVisitedLoaded(
      /*time=*/base::Seconds(1), /*using_most_visited=*/false,
      /*using_custom_links=*/true, /*using_enterprise_shortcuts=*/false,
      /*is_visible=*/true, /*is_expanded=*/true);
  EXPECT_THAT(histogram_tester.GetAllSamples("Omnibox.NumberOfTiles"),
              ElementsAre(Bucket(2, 1)));
}

TEST(MostVisitedMetricsLoggerTest, ShouldRecordNavigation) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  MostVisitedMetricsLogger logger("Omnibox");

  logger.LogMostVisitedNavigation(
      MakeImpression(0, ntp_tiles::TileSource::TOP_SITES));

  EXPECT_THAT(histogram_tester.GetAllSamples("Omnibox.MostVisited"),
              ElementsAre(Bucket(0, 1)));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MostVisited_Clicked"));
}

}  // namespace
