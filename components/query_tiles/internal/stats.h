// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_

#include "components/query_tiles/internal/tile_types.h"

namespace query_tiles {
namespace stats {

extern const char kImagePreloadingHistogram[];

extern const char kHttpResponseCodeHistogram[];

extern const char kNetErrorCodeHistogram[];

extern const char kRequestStatusHistogram[];

extern const char kGroupStatusHistogram[];

extern const char kFirstFlowDurationHistogram[];

extern const char kFetcherStartHourHistogram[];

extern const char kPrunedGroupReasonHistogram[];

extern const char kTrendingTileEventHistogram[];

// Event to track image loading metrics.
enum class ImagePreloadingEvent {
  // Start to fetch image in full browser mode.
  kStart = 0,
  // Fetch success in full browser mode.
  kSuccess = 1,
  // Fetch failure in full browser mode.
  kFailure = 2,
  // Start to fetch image in reduced mode.
  kStartReducedMode = 3,
  // Fetch success in reduced mode.
  kSuccessReducedMode = 4,
  // Fetch failure in reduced mode.
  kFailureReducedMode = 5,
  kMaxValue = kFailureReducedMode,
};

enum class PrunedGroupReason {
  // Group has expired.
  kExpired = 0,
  // Locale mismatched.
  kInvalidLocale = 1,
  kMaxValue = kInvalidLocale,
};

// Event to track trending tile metrics.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TrendingTileEvent {
  // A trending tile is shown.
  kShown = 0,
  // A trending tile is removed.
  kRemoved = 1,
  // A trending tile is clicked.
  kClicked = 2,
  kMaxValue = kClicked,
};

// Records an image preloading event.
void RecordImageLoading(ImagePreloadingEvent event);

// Records HTTP response code.
void RecordTileFetcherResponseCode(int response_code);

// Records net error code.
void RecordTileFetcherNetErrorCode(int error_code);

// Records request result from tile fetcher.
void RecordTileRequestStatus(TileInfoRequestStatus status);

// Records status of tile group.
void RecordTileGroupStatus(TileGroupStatus status);

// Records the number of hours passed from first time schedule to first time
// run.
void RecordFirstFetchFlowDuration(int hours);

// Records the locale explode hour when fetching starts.
void RecordExplodeOnFetchStarted(int explode);

// Records the reason to cause TileManager to prune the group.
void RecordGroupPruned(PrunedGroupReason reason);

// Records the event for trending tile.
void RecordTrendingTileEvent(TrendingTileEvent event);

}  // namespace stats
}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_
