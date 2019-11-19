// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/ntp_tiles/constants.h"

namespace ntp_tiles {
namespace metrics {

namespace {

const int kLastTitleSource = static_cast<int>(TileTitleSource::LAST);

// Identifiers for the various tile sources.
const char kHistogramClientName[] = "client";
const char kHistogramServerName[] = "server";
const char kHistogramPopularName[] = "popular_fetched";
const char kHistogramBakedInName[] = "popular_baked_in";
const char kHistogramWhitelistName[] = "whitelist";
const char kHistogramHomepageName[] = "homepage";
const char kHistogramCustomLinksName[] = "custom_links";
const char kHistogramExploreName[] = "explore";

// Suffixes for the various icon types.
const char kTileTypeSuffixIconColor[] = "IconsColor";
const char kTileTypeSuffixIconGray[] = "IconsGray";
const char kTileTypeSuffixIconReal[] = "IconsReal";

void LogUmaHistogramAge(const std::string& name, const base::TimeDelta& value) {
  // Log the value in number of seconds.
  base::UmaHistogramCustomCounts(name, value.InSeconds(), 5,
                                 base::TimeDelta::FromDays(14).InSeconds(), 20);
}

std::string GetSourceHistogramName(TileSource source) {
  switch (source) {
    case TileSource::TOP_SITES:
      return kHistogramClientName;
    case TileSource::POPULAR_BAKED_IN:
      return kHistogramBakedInName;
    case TileSource::POPULAR:
      return kHistogramPopularName;
    case TileSource::WHITELIST:
      return kHistogramWhitelistName;
    case TileSource::SUGGESTIONS_SERVICE:
      return kHistogramServerName;
    case TileSource::HOMEPAGE:
      return kHistogramHomepageName;
    case TileSource::CUSTOM_LINKS:
      return kHistogramCustomLinksName;
    case TileSource::EXPLORE:
      return kHistogramExploreName;
  }
  NOTREACHED();
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
    case TileVisualType::NONE:                     // Fall through.
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

  if (!impression.data_generation_time.is_null()) {
    const base::TimeDelta age =
        base::Time::Now() - impression.data_generation_time;
    LogUmaHistogramAge("NewTabPage.SuggestionsImpressionAge", age);
    LogUmaHistogramAge(
        base::StringPrintf("NewTabPage.SuggestionsImpressionAge.%s",
                           source_name.c_str()),
        age);
  }

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

    if (impression.icon_type != favicon_base::IconType::kInvalid) {
      base::UmaHistogramEnumeration(
          base::StringPrintf("NewTabPage.TileFaviconType.%s", tile_type_suffix),
          impression.icon_type, favicon_base::IconType::kCount);
    }
  }

  if (impression.icon_type != favicon_base::IconType::kInvalid) {
    base::UmaHistogramEnumeration("NewTabPage.TileFaviconType",
                                  impression.icon_type,
                                  favicon_base::IconType::kCount);
  }
}

void RecordTileClick(const NTPTileImpression& impression) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisited", impression.index,
                            kMaxNumTiles);

  std::string source_name = GetSourceHistogramName(impression.source);
  base::UmaHistogramExactLinear(
      base::StringPrintf("NewTabPage.MostVisited.%s", source_name.c_str()),
      impression.index, kMaxNumTiles);

  if (!impression.data_generation_time.is_null()) {
    const base::TimeDelta age =
        base::Time::Now() - impression.data_generation_time;
    LogUmaHistogramAge("NewTabPage.MostVisitedAge", age);
    LogUmaHistogramAge(
        base::StringPrintf("NewTabPage.MostVisitedAge.%s", source_name.c_str()),
        age);
  }

  const char* tile_type_suffix = GetTileTypeSuffix(impression.visual_type);
  if (tile_type_suffix) {
    base::UmaHistogramExactLinear(
        base::StringPrintf("NewTabPage.MostVisited.%s", tile_type_suffix),
        impression.index, kMaxNumTiles);

    if (impression.icon_type != favicon_base::IconType::kInvalid) {
      base::UmaHistogramEnumeration(
          base::StringPrintf("NewTabPage.TileFaviconTypeClicked.%s",
                             tile_type_suffix),
          impression.icon_type, favicon_base::IconType::kCount);
    }
  }

  if (impression.icon_type != favicon_base::IconType::kInvalid) {
    base::UmaHistogramEnumeration("NewTabPage.TileFaviconTypeClicked",
                                  impression.icon_type,
                                  favicon_base::IconType::kCount);
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

}  // namespace metrics
}  // namespace ntp_tiles
