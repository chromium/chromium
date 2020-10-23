// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_SWITCHES_H_
#define COMPONENTS_QUERY_TILES_SWITCHES_H_

#include "base/feature_list.h"

namespace query_tiles {

namespace features {

// Main feature flag for the query tiles feature that allows or blocks the
// feature in the user's country. Must be checked in addition to any other flag.
extern const base::Feature kQueryTilesGeoFilter;

// Main feature flag for the query tiles feature. All other flags are
// effective only when this flag is enabled.
extern const base::Feature kQueryTiles;

// Feature flag to determine whether query tiles should be shown on NTP.
extern const base::Feature kQueryTilesInNTP;

// Feature flag to determine whether query tiles should be shown on omnibox.
extern const base::Feature kQueryTilesInOmnibox;

// Feature flag to determine whether the user will have a chance to edit the
// query before in the omnibox sumbitting the search. In this mode only one
// level of tiles will be displayed.
extern const base::Feature kQueryTilesEnableQueryEditing;

// Feature flag to determine whether query tiles should be displayed in an order
// based on local user interactions.
extern const base::Feature kQueryTilesLocalOrdering;

// Helper function to determine whether query tiles should be shown on omnibox.
bool IsEnabledQueryTilesInOmnibox();

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

}  // namespace switches
}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_SWITCHES_H_
