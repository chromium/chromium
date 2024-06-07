// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "components/ntp_tiles/constants.h"

namespace ntp_tiles {
namespace metrics {

namespace {

const int kLastTitleSource = static_cast<int>(TileTitleSource::LAST);

// Identifiers for the various tile sources. Should sync with
// NewTabPageProviders in histogram_suffixes_list.xml.
const char kHistogramClientName[] = "client";
const char kHistogramPopularName[] = "popular_fetched";
const char kHistogramBakedInName[] = "popular_baked_in";
const char kHistogramAllowlistName[] = "allowlist";
const char kHistogramHomepageName[] = "homepage";
const char kHistogramCustomLinksName[] = "custom_links";

// Suffixes for the various icon types.
const char kTileTypeSuffixIconColor[] = "IconsColor";
const char kTileTypeSuffixIconGray[] = "IconsGray";
const char kTileTypeSuffixIconReal[] = "IconsReal";

std::string GetSourceHistogramName(TileSource source) {
  switch (source) {
    case TileSource::TOP_SITES:
      return kHistogramClientName;
    case TileSource::POPULAR_BAKED_IN:
      return kHistogramBakedInName;
    case TileSource::POPULAR:
      return kHistogramPopularName;
    case TileSource::ALLOWLIST:
      return kHistogramAllowlistName;
    case TileSource::HOMEPAGE:
      return kHistogramHomepageName;
    case TileSource::CUSTOM_LINKS:
      return kHistogramCustomLinksName;
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

const char* GetTileTypeSuffix(TileVisualType type) {
  switch (type) {
    case TileVisualType::ICON_COLOR:
      return kTileTypeSuffixIconColor;
    case TileVisualType::ICON_DEFAULT:
      return kTileTypeSuffixIconGray;
    case TileVisualType::ICON_REAL:
      return kTileTypeSuffixIconReal;
    case TileVisualType::NONE:  // Fall through.
    case TileVisualType::UNKNOWN_TILE_TYPE:
      break;
  }
  return nullptr;
}

}  // namespace

void RecordPageImpression(int number_of_tiles) {
  base::UmaHistogramSparse("NewTabPage.NumberOfTiles", number_of_tiles);
}

void RecordTileImpression(const NTPTileImpression& impression) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.SuggestionsImpression",
                            impression.index, kMaxNumTiles);

  std::string source_name = GetSourceHistogramName(impression.source);
  base::UmaHistogramExactLinear(
      base::StringPrintf("NewTabPage.SuggestionsImpression.%s",
                         source_name.c_str()),
      impression.index, kMaxNumTiles);

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileTitle",
                            static_cast<int>(impression.title_source),
                            kLastTitleSource + 1);
  base::UmaHistogramExactLinear(
      base::StringPrintf("NewTabPage.TileTitle.%s",
                         GetSourceHistogramName(impression.source).c_str()),
      static_cast<int>(impression.title_source), kLastTitleSource + 1);

  if (impression.visual_type > LAST_RECORDED_TILE_TYPE) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileType", impression.visual_type,
                            LAST_RECORDED_TILE_TYPE + 1);

  base::UmaHistogramExactLinear(
      base::StringPrintf("NewTabPage.TileType.%s", source_name.c_str()),
      impression.visual_type, LAST_RECORDED_TILE_TYPE + 1);

  const char* tile_type_suffix = GetTileTypeSuffix(impression.visual_type);
  if (tile_type_suffix) {
    // TODO(http://crbug.com/1021598): Add UKM here.
    base::UmaHistogramExactLinear(
        base::StringPrintf("NewTabPage.SuggestionsImpression.%s",
                           tile_type_suffix),
        impression.index, kMaxNumTiles);
  }
}

void RecordTileClick(const NTPTileImpression& impression) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisited", impression.index,
                            kMaxNumTiles);
  base::RecordAction(base::UserMetricsAction("NewTabPage.MostVisited.Clicked"));

  std::string source_name = GetSourceHistogramName(impression.source);
  base::UmaHistogramExactLinear(
      base::StringPrintf("NewTabPage.MostVisited.%s", source_name.c_str()),
      impression.index, kMaxNumTiles);

  const char* tile_type_suffix = GetTileTypeSuffix(impression.visual_type);
  if (tile_type_suffix) {
    base::UmaHistogramExactLinear(
        base::StringPrintf("NewTabPage.MostVisited.%s", tile_type_suffix),
        impression.index, kMaxNumTiles);
  }

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileTitleClicked",
                            static_cast<int>(impression.title_source),
                            kLastTitleSource + 1);
  base::UmaHistogramExactLinear(
      base::StringPrintf("NewTabPage.TileTitleClicked.%s",
                         GetSourceHistogramName(impression.source).c_str()),
      static_cast<int>(impression.title_source), kLastTitleSource + 1);

  if (impression.visual_type <= LAST_RECORDED_TILE_TYPE) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileTypeClicked",
                              impression.visual_type,
                              LAST_RECORDED_TILE_TYPE + 1);

    base::UmaHistogramExactLinear(
        base::StringPrintf("NewTabPage.TileTypeClicked.%s",
                           GetSourceHistogramName(impression.source).c_str()),
        impression.visual_type, LAST_RECORDED_TILE_TYPE + 1);
  }
}

void RecordsMigratedDefaultAppDeleted(
    const DeletedTileType& most_visited_app_type) {
  base::UmaHistogramEnumeration("NewTabPage.MostVisitedMigratedDefaultAppType",
                                most_visited_app_type);
}

}  // namespace metrics
}  // namespace ntp_tiles
