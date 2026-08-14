// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
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
const char kHistogramEnterpriseShortcutsName[] = "enterprise_shortcuts";

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
    case TileSource::ENTERPRISE_SHORTCUTS:
      return kHistogramEnterpriseShortcutsName;
  }
  NOTREACHED();
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

void RecordPageImpression(int number_of_tiles, std::string_view prefix) {
  base::UmaHistogramSparse(base::StrCat({prefix, ".NumberOfTiles"}),
                           number_of_tiles);
}

void RecordNumberOfCustomTilesOnFirstNtp(int number_of_custom_tiles,
                                         std::string_view prefix) {
  base::UmaHistogramSparse(
      base::StrCat({prefix, ".MostVisited.NumberOfCustomTilesOnFirstNtp"}),
      number_of_custom_tiles);
}

void RecordTileImpression(const NTPTileImpression& impression,
                          std::string_view prefix) {
  base::UmaHistogramExactLinear(
      base::StrCat({prefix, ".SuggestionsImpression"}), impression.index,
      kMaxNumTiles);

  std::string source_name = GetSourceHistogramName(impression.source);
  base::UmaHistogramExactLinear(
      base::StrCat({prefix, ".SuggestionsImpression.", source_name}),
      impression.index, kMaxNumTiles);

  base::UmaHistogramExactLinear(base::StrCat({prefix, ".TileTitle"}),
                                static_cast<int>(impression.title_source),
                                kLastTitleSource + 1);
  base::UmaHistogramExactLinear(
      base::StrCat({prefix, ".TileTitle.", source_name}),
      static_cast<int>(impression.title_source), kLastTitleSource + 1);

  if (impression.visual_type > LAST_RECORDED_TILE_TYPE) {
    return;
  }

  base::UmaHistogramExactLinear(base::StrCat({prefix, ".TileType"}),
                                impression.visual_type,
                                LAST_RECORDED_TILE_TYPE + 1);

  base::UmaHistogramExactLinear(
      base::StrCat({prefix, ".TileType.", source_name}), impression.visual_type,
      LAST_RECORDED_TILE_TYPE + 1);

  const char* tile_type_suffix = GetTileTypeSuffix(impression.visual_type);
  if (tile_type_suffix) {
    // TODO(http://crbug.com/1021598): Add UKM here.
    base::UmaHistogramExactLinear(
        base::StrCat({prefix, ".SuggestionsImpression.", tile_type_suffix}),
        impression.index, kMaxNumTiles);
  }
}

void RecordTileClick(const NTPTileImpression& impression,
                     std::string_view prefix) {
  base::UmaHistogramExactLinear(base::StrCat({prefix, ".MostVisited"}),
                                impression.index, kMaxNumTiles);
  base::RecordComputedAction(base::StrCat({prefix, ".MostVisited.Clicked"}));

  std::string source_name = GetSourceHistogramName(impression.source);
  base::UmaHistogramExactLinear(
      base::StrCat({prefix, ".MostVisited.", source_name}), impression.index,
      kMaxNumTiles);

  const char* tile_type_suffix = GetTileTypeSuffix(impression.visual_type);
  if (tile_type_suffix) {
    base::UmaHistogramExactLinear(
        base::StrCat({prefix, ".MostVisited.", tile_type_suffix}),
        impression.index, kMaxNumTiles);
  }

  base::UmaHistogramExactLinear(base::StrCat({prefix, ".TileTitleClicked"}),
                                static_cast<int>(impression.title_source),
                                kLastTitleSource + 1);
  base::UmaHistogramExactLinear(
      base::StrCat({prefix, ".TileTitleClicked.", source_name}),
      static_cast<int>(impression.title_source), kLastTitleSource + 1);

  if (impression.visual_type <= LAST_RECORDED_TILE_TYPE) {
    base::UmaHistogramExactLinear(base::StrCat({prefix, ".TileTypeClicked"}),
                                  impression.visual_type,
                                  LAST_RECORDED_TILE_TYPE + 1);

    base::UmaHistogramExactLinear(
        base::StrCat({prefix, ".TileTypeClicked.", source_name}),
        impression.visual_type, LAST_RECORDED_TILE_TYPE + 1);
  }
}

void RecordsMigratedDefaultAppDeleted(const TileType& most_visited_app_type) {
  base::UmaHistogramEnumeration("NewTabPage.MostVisitedMigratedDefaultAppType",
                                most_visited_app_type);
}

}  // namespace metrics
}  // namespace ntp_tiles
