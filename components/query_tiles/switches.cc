// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/switches.h"

#include "base/strings/string_util.h"

namespace query_tiles {
namespace features {
const base::Feature kQueryTiles{"QueryTiles",
                                base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kQueryTilesInNTP{"QueryTilesInNTP",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kQueryTilesEnableQueryEditing{
    "QueryTilesEnableQueryEditing", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kQueryTilesRemoveTrendingTilesAfterInactivity{
    "QueryTilesRemoveTrendingAfterInactivity",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kQueryTilesSegmentation{"QueryTilesSegmentation",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kQueryTilesDisableCountryOverride{
    "QueryTilesDisableCountryOverride", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kQueryTilesOnStart{
    "QueryTilesOnStart", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsQueryTilesEnabledForCountry(const std::string& country_code) {
  std::string enabled_countries[] = {"IN", "NG", "JP"};
  for (const auto& country : enabled_countries) {
    if (base::EqualsCaseInsensitiveASCII(country_code, country))
      return true;
  }
  return false;
}

}  // namespace features

namespace switches {
const char kQueryTilesSingleTier[] = "query-tiles-single-tier";

const char kQueryTilesCountryCode[] = "query-tiles-country-code";

const char kQueryTilesInstantBackgroundTask[] =
    "query-tiles-instant-background-task";

const char kQueryTilesEnableTrending[] = "query-tiles-enable-trending";

const char kQueryTilesRankTiles[] = "query-tiles-rank-tiles";
}  // namespace switches
}  // namespace query_tiles
