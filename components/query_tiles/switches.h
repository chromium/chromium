// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_SWITCHES_H_
#define COMPONENTS_QUERY_TILES_SWITCHES_H_

#include "base/feature_list.h"

namespace query_tiles {

namespace features {

// Main feature flag for the query tiles feature. All other flags are
// effective only when this flag is enabled.
extern const base::Feature kQueryTiles;

// Feature flag to determine whether query tiles should be shown on NTP.
extern const base::Feature kQueryTilesInNTP;

// Feature flag to determine whether the user will have a chance to edit the
// query before in the omnibox sumbitting the search. In this mode only one
// level of tiles will be displayed.
extern const base::Feature kQueryTilesEnableQueryEditing;

// Feature flag to determine whether trending tiles should disapear after
// some time of inactivity.
extern const base::Feature kQueryTilesRemoveTrendingTilesAfterInactivity;

// Whether segmentation rules are applied to query tiles.
extern const base::Feature kQueryTilesSegmentation;

// Whether to disable the override rules introduced for countries.
extern const base::Feature kQueryTilesDisableCountryOverride;

// Feature flag to determine whether query tiles should be shown on start surface.
extern const base::Feature kQueryTilesOnStart;

// Returns whether query tiles are enabled for the country.
bool IsQueryTilesEnabledForCountry(const std::string& country_code);
}  // namespace features

namespace switches {

// If set, only one level of query tiles will be shown.
extern const char kQueryTilesSingleTier[];

// If set, this value overrides the default country code to be sent to the
// server when fetching tiles.
extern const char kQueryTilesCountryCode[];

// If set, the background task will be started after a short period.
extern const char kQueryTilesInstantBackgroundTask[];

// If set, server will return trending tiles along with curated tiles.
extern const char kQueryTilesEnableTrending[];

// If set, the server will rank all the tiles and send a subset of them
// to the client based on user interest.
extern const char kQueryTilesRankTiles[];
}  // namespace switches
}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_SWITCHES_H_
