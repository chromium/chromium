// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/ntp_user_data_logger.h"

#include <memory>
#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ui/search/ntp_user_data_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/url_constants.h"
#include "components/favicon_base/favicon_types.h"
#include "components/ntp_tiles/constants.h"
#include "components/search/ntp_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using ntp_tiles::TileSource;
using ntp_tiles::TileTitleSource;
using ntp_tiles::TileVisualType;
using testing::ContainerEq;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

namespace {

constexpr int kUnknownTitleSource = static_cast<int>(TileTitleSource::UNKNOWN);
constexpr int kManifestTitleSource =
    static_cast<int>(TileTitleSource::MANIFEST);
constexpr int kMetaTagTitleSource = static_cast<int>(TileTitleSource::META_TAG);
constexpr int kTitleTagTitleSource =
    static_cast<int>(TileTitleSource::TITLE_TAG);
constexpr int kInferredTitleSource =
    static_cast<int>(TileTitleSource::INFERRED);

using Sample = base::HistogramBase::Sample;
using Samples = std::vector<Sample>;

// Helper function that uses sensible defaults for irrelevant fields of
// NTPTileImpression.
ntp_tiles::NTPTileImpression MakeNTPTileImpression(int index,
                                                   TileSource source,
                                                   TileTitleSource title_source,
                                                   TileVisualType visual_type) {
  return ntp_tiles::NTPTileImpression(index, source, title_source, visual_type,
                                      favicon_base::IconType::kInvalid,
                                      /*url_for_rappor=*/GURL());
}

// Helper function that populates a list of expected impressions, each with the
// expected |count|.
std::vector<base::Bucket> FillImpressions(int numImpressions, int count) {
  std::vector<base::Bucket> impressions;
  for (int i = 0; i < numImpressions; ++i) {
    impressions.push_back(Bucket(i, count));
  }
  return impressions;
}

class TestNTPUserDataLogger : public NTPUserDataLogger {
 public:
  explicit TestNTPUserDataLogger(const GURL& ntp_url)
      : NTPUserDataLogger(nullptr, ntp_url, base::Time::Now()) {}

  ~TestNTPUserDataLogger() override {}

  bool DefaultSearchProviderIsGoogle() const override { return is_google_; }

  bool CustomBackgroundIsConfigured() const override {
    return is_custom_background_configured_;
  }

  bool is_custom_background_configured_ = false;
  bool is_google_ = true;
};

using NTPUserDataLoggerTest = testing::Test;

MATCHER_P3(IsBucketBetween, lower_bound, upper_bound, count, "") {
  return arg.min >= lower_bound && arg.min <= upper_bound && arg.count == count;
}

}  // namespace

TEST_F(NTPUserDataLoggerTest, ShouldRecordNumberOfTiles) {
  base::HistogramTester histogram_tester;

  // Ensure non-zero statistics.
  TestNTPUserDataLogger logger(GURL("chrome://newtab/"));

  const base::TimeDelta delta = base::Milliseconds(73);

  for (int i = 0; i < ntp_tiles::kMaxNumTiles; ++i) {
    logger.LogMostVisitedImpression(MakeNTPTileImpression(
        i, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
        TileVisualType::ICON_REAL));
  }
  logger.LogMostVisitedLoaded(delta, /*using_most_visited=*/true,
                              /*is_visible=*/true);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.NumberOfTiles"),
              ElementsAre(Bucket(ntp_tiles::kMaxNumTiles, 1)));

  // We should not log again for the same NTP.
  logger.LogMostVisitedLoaded(delta, /*using_most_visited=*/true,
                              /*is_visible=*/true);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.NumberOfTiles"),
              ElementsAre(Bucket(ntp_tiles::kMaxNumTiles, 1)));
}

TEST_F(NTPUserDataLoggerTest, ShouldNotRecordImpressionsBeforeAllTilesLoaded) {
  TestNTPUserDataLogger logger(GURL("chrome://newtab/"));

  base::HistogramTester histogram_tester;

  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(1, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_DEFAULT));

  // The actual histograms are emitted only after the ALL_TILES_LOADED event,
  // so at this point everything should still be empty.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType"), IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle.client"),
              IsEmpty());
}

TEST_F(NTPUserDataLoggerTest, ShouldRecordImpressions) {
  TestNTPUserDataLogger logger(GURL("chrome://newtab/"));

  base::HistogramTester histogram_tester;

  // Impressions increment the associated bins.
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(0, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(1, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_DEFAULT));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(2, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(3, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(MakeNTPTileImpression(
      4, TileSource::TOP_SITES, TileTitleSource::TITLE_TAG,
      TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(5, TileSource::TOP_SITES, TileTitleSource::MANIFEST,
                            TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(6, TileSource::POPULAR, TileTitleSource::TITLE_TAG,
                            TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(MakeNTPTileImpression(
      7, TileSource::POPULAR_BAKED_IN, TileTitleSource::META_TAG,
      TileVisualType::ICON_REAL));

  // This should trigger emitting histograms.
  logger.LogMostVisitedLoaded(base::Milliseconds(73),
                              /*using_most_visited=*/true, /*is_visible=*/true);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1),
                  Bucket(4, 1), Bucket(5, 1), Bucket(6, 1), Bucket(7, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1),
                  Bucket(4, 1), Bucket(5, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.popular_fetched"),
              ElementsAre(Bucket(6, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.popular_baked_in"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType"),
              ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 7),
                          Bucket(ntp_tiles::TileVisualType::ICON_DEFAULT, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 5),
                          Bucket(ntp_tiles::TileVisualType::ICON_DEFAULT, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType.popular_fetched"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType.popular_baked_in"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle"),
              ElementsAre(Bucket(kManifestTitleSource, 1),
                          Bucket(kMetaTagTitleSource, 1),
                          Bucket(kTitleTagTitleSource, 2),
                          Bucket(kInferredTitleSource, 4)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle.client"),
              ElementsAre(Bucket(kManifestTitleSource, 1),
                          Bucket(kTitleTagTitleSource, 1),
                          Bucket(kInferredTitleSource, 4)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTitle.popular_fetched"),
      ElementsAre(Bucket(kTitleTagTitleSource, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTitle.popular_baked_in"),
      ElementsAre(Bucket(kMetaTagTitleSource, 1)));
}

TEST_F(NTPUserDataLoggerTest, ShouldNotRecordRepeatedImpressions) {
  TestNTPUserDataLogger logger(GURL("chrome://newtab/"));

  base::HistogramTester histogram_tester;

  // Impressions increment the associated bins.
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(0, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(1, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_DEFAULT));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(2, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(3, TileSource::TOP_SITES, TileTitleSource::INFERRED,
                            TileVisualType::ICON_REAL));

  // Repeated impressions for the same bins are ignored.
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(0, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
                            TileVisualType::ICON_DEFAULT));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(1, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
                            TileVisualType::ICON_DEFAULT));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(2, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
                            TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(3, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
                            TileVisualType::ICON_REAL));

  // This should trigger emitting histograms.
  logger.LogMostVisitedLoaded(base::Milliseconds(73),
                              /*using_most_visited=*/true, /*is_visible=*/true);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType"),
              ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 3),
                          Bucket(ntp_tiles::TileVisualType::ICON_DEFAULT, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 3),
                          Bucket(ntp_tiles::TileVisualType::ICON_DEFAULT, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle"),
              ElementsAre(Bucket(kInferredTitleSource, 4)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle.client"),
              ElementsAre(Bucket(kInferredTitleSource, 4)));
}

TEST_F(NTPUserDataLoggerTest, ShouldNotRecordImpressionsForBinsBeyondMax) {
  TestNTPUserDataLogger logger(GURL("chrome://newtab/"));

  base::HistogramTester histogram_tester;

  // Impressions increment the associated bins.
  for (int bin = 0; bin < ntp_tiles::kMaxNumTiles; bin++) {
    logger.LogMostVisitedImpression(MakeNTPTileImpression(
        bin, TileSource::TOP_SITES, TileTitleSource::INFERRED,
        TileVisualType::ICON_REAL));
  }

  // Impressions are silently ignored for tiles >= ntp_tiles::kMaxNumTiles.
  logger.LogMostVisitedImpression(MakeNTPTileImpression(
      ntp_tiles::kMaxNumTiles, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
      TileVisualType::ICON_REAL));
  logger.LogMostVisitedImpression(MakeNTPTileImpression(
      ntp_tiles::kMaxNumTiles + 1, TileSource::TOP_SITES,
      TileTitleSource::UNKNOWN, TileVisualType::ICON_DEFAULT));

  // This should trigger emitting histograms.
  logger.LogMostVisitedLoaded(base::Milliseconds(73),
                              /*using_most_visited=*/true, /*is_visible=*/true);

  std::vector<base::Bucket> expectedImpressions =
      FillImpressions(ntp_tiles::kMaxNumTiles, 1);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      ContainerEq(expectedImpressions));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      ContainerEq(expectedImpressions));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType"),
              ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL,
                                 ntp_tiles::kMaxNumTiles)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL,
                                 ntp_tiles::kMaxNumTiles)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTitle"),
      ElementsAre(Bucket(kInferredTitleSource, ntp_tiles::kMaxNumTiles)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTitle.client"),
      ElementsAre(Bucket(kInferredTitleSource, ntp_tiles::kMaxNumTiles)));
}

TEST_F(NTPUserDataLoggerTest, ShouldRecordNavigations) {
  TestNTPUserDataLogger logger(GURL("chrome://newtab/"));

  {
    base::HistogramTester histogram_tester;

    logger.LogMostVisitedNavigation(MakeNTPTileImpression(
        0, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
        TileVisualType::ICON_REAL));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
                ElementsAre(Bucket(0, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
                ElementsAre(Bucket(0, 1)));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
                ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
        ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 1)));
  }

  {
    base::HistogramTester histogram_tester;

    logger.LogMostVisitedNavigation(MakeNTPTileImpression(
        1, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
        TileVisualType::ICON_DEFAULT));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
                ElementsAre(Bucket(1, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
                ElementsAre(Bucket(1, 1)));

    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
        ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_DEFAULT, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
        ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_DEFAULT, 1)));
  }

  {
    base::HistogramTester histogram_tester;

    logger.LogMostVisitedNavigation(MakeNTPTileImpression(
        2, TileSource::TOP_SITES, TileTitleSource::MANIFEST,
        TileVisualType::ICON_DEFAULT));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
                ElementsAre(Bucket(2, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
                ElementsAre(Bucket(2, 1)));

    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
        ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_DEFAULT, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
        ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_DEFAULT, 1)));
  }

  {
    base::HistogramTester histogram_tester;

    logger.LogMostVisitedNavigation(
        MakeNTPTileImpression(3, TileSource::POPULAR, TileTitleSource::META_TAG,
                              TileVisualType::ICON_REAL));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
                ElementsAre(Bucket(3, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
                IsEmpty());
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "NewTabPage.MostVisited.popular_fetched"),
                ElementsAre(Bucket(3, 1)));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
                ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
        IsEmpty());
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "NewTabPage.TileTypeClicked.popular_fetched"),
                ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 1)));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitleClicked"),
                ElementsAre(Bucket(kMetaTagTitleSource, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTitleClicked.client"),
        IsEmpty());
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "NewTabPage.TileTitleClicked.popular_fetched"),
                ElementsAre(Bucket(kMetaTagTitleSource, 1)));
  }

  {
    base::HistogramTester histogram_tester;

    // Navigations always increase.
    logger.LogMostVisitedNavigation(MakeNTPTileImpression(
        0, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
        TileVisualType::ICON_REAL));
    logger.LogMostVisitedNavigation(MakeNTPTileImpression(
        1, TileSource::TOP_SITES, TileTitleSource::TITLE_TAG,
        TileVisualType::ICON_REAL));
    logger.LogMostVisitedNavigation(MakeNTPTileImpression(
        2, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
        TileVisualType::ICON_REAL));
    logger.LogMostVisitedNavigation(
        MakeNTPTileImpression(3, TileSource::POPULAR, TileTitleSource::MANIFEST,
                              TileVisualType::ICON_REAL));

    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
        ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
                ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "NewTabPage.MostVisited.popular_fetched"),
                ElementsAre(Bucket(3, 1)));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
                ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 4)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
        ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 3)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "NewTabPage.TileTypeClicked.popular_fetched"),
                ElementsAre(Bucket(ntp_tiles::TileVisualType::ICON_REAL, 1)));

    EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitleClicked"),
                ElementsAre(Bucket(kUnknownTitleSource, 2),
                            Bucket(kManifestTitleSource, 1),
                            Bucket(kTitleTagTitleSource, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("NewTabPage.TileTitleClicked.client"),
        ElementsAre(Bucket(kUnknownTitleSource, 2),
                    Bucket(kTitleTagTitleSource, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "NewTabPage.TileTitleClicked.popular_fetched"),
                ElementsAre(Bucket(kManifestTitleSource, 1)));
  }
}

TEST_F(NTPUserDataLoggerTest, ShouldRecordMostVisitedLoadTime) {
  base::HistogramTester histogram_tester;

  TestNTPUserDataLogger logger(GURL("chrome://newtab/"));

  base::TimeDelta delta_tiles_loaded = base::Milliseconds(100);

  // Log a TOP_SITES impression (for the .MostVisited vs .MostLikely split in
  // the time histograms).
  logger.LogMostVisitedImpression(
      MakeNTPTileImpression(0, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
                            TileVisualType::ICON_REAL));

  // This should trigger emitting histograms.
  logger.LogMostVisitedLoaded(delta_tiles_loaded, /*using_most_visited=*/true,
                              /*is_visible=*/true);

  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime"), SizeIs(1));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime.MostVisited"),
              SizeIs(1));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime.MostLikely"),
              IsEmpty());

  histogram_tester.ExpectTimeBucketCount("NewTabPage.LoadTime",
                                         delta_tiles_loaded, 1);
  histogram_tester.ExpectTimeBucketCount("NewTabPage.LoadTime.MostVisited",
                                         delta_tiles_loaded, 1);

  // We should not log again for the same NTP.
  logger.LogMostVisitedLoaded(delta_tiles_loaded, /*using_most_visited=*/true,
                              /*is_visible=*/true);
  histogram_tester.ExpectTimeBucketCount("NewTabPage.LoadTime",
                                         delta_tiles_loaded, 1);
}

TEST_F(NTPUserDataLoggerTest, ShouldRecordImpressionsAge) {
  base::HistogramTester histogram_tester;

  // Ensure non-zero statistics.
  TestNTPUserDataLogger logger(GURL("chrome://newtab/"));

  constexpr base::TimeDelta delta = base::Milliseconds(0);

  logger.LogMostVisitedImpression(ntp_tiles::NTPTileImpression(
      0, TileSource::TOP_SITES, TileTitleSource::UNKNOWN,
      TileVisualType::ICON_REAL, favicon_base::IconType::kInvalid, GURL()));

  logger.LogMostVisitedLoaded(delta, /*using_most_visited=*/true,
                              /*is_visible=*/true);
}

TEST_F(NTPUserDataLoggerTest,
       ShouldNotRecordShortcutsAreCustomizedFromNTPOther) {
  base::HistogramTester histogram_tester;

  TestNTPUserDataLogger logger(GURL("https://www.notgoogle.com/newtab"));
  logger.is_google_ = false;

  base::TimeDelta delta_tiles_loaded = base::Milliseconds(100);

  // This should trigger emitting histograms.
  logger.LogMostVisitedLoaded(delta_tiles_loaded, /*using_most_visited=*/false,
                              /*is_visible=*/true);

  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.Customized"),
              IsEmpty());
}

TEST_F(NTPUserDataLoggerTest,
       ShouldNotRecordCustomizedShortcutSettingsFromNTPOther) {
  base::HistogramTester histogram_tester;

  TestNTPUserDataLogger logger(GURL("https://www.notgoogle.com/newtab"));
  logger.is_google_ = false;

  base::TimeDelta delta_tiles_loaded = base::Milliseconds(100);

  // This should trigger emitting histograms.
  logger.LogMostVisitedLoaded(delta_tiles_loaded, /*using_most_visited=*/true,
                              /*is_visible=*/true);

  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.CustomizedShortcuts"),
              IsEmpty());
}

TEST_F(NTPUserDataLoggerTest, ShouldNotRecordCustomizationActionFromNTPOther) {
  base::HistogramTester histogram_tester;

  TestNTPUserDataLogger logger(GURL("https://www.notgoogle.com/newtab"));
  logger.is_google_ = false;

  base::TimeDelta delta_tiles_loaded = base::Milliseconds(100);

  // This should trigger emitting histograms.
  logger.LogMostVisitedLoaded(delta_tiles_loaded, /*using_most_visited=*/true,
                              /*is_visible=*/true);

  // Attempt to log an event that is only supported when the default search
  // provider is Google.
  logger.LogEvent(NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION,
                  delta_tiles_loaded);

  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime"), SizeIs(1));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime.WebUINTP"),
              IsEmpty());

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.CustomizeChromeBackgroundAction"),
              IsEmpty());
}
