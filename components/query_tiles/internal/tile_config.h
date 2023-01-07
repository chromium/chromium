// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/query_tiles/internal/tile_types.h"
#include "url/gurl.h"

namespace query_tiles {

// Default URL string for GetQueryTiles RPC.
extern const char kDefaultGetQueryTilePath[];

// Finch parameter key for experiment tag to be passed to the server.
extern const char kExperimentTagKey[];

// Finch parameter key for base server URL to retrieve the tiles.
extern const char kBaseURLKey[];

// Finch parameter key for expire duration in seconds.
extern const char kExpireDurationKey[];

// Finch parameter key for expire duration in seconds.
extern const char kIsUnmeteredNetworkRequiredKey[];

// Finch parameter key for image prefetch mode.
extern const char kImagePrefetchModeKey[];

// Finch parameter key for the minimum interval to next schedule.
extern const char kScheduleIntervalKey[];

// Finch parameter key for random window.
extern const char kMaxRandomWindowKey[];

// Finch parameter key for one off task window.
extern const char kOneoffTaskWindowKey[];

// Finch parameter key for Backoff policy initial delay in ms.
extern const char kBackoffInitDelayInMsKey[];

// Finch parameter key for Backoff policy maximum delay in ms.
extern const char kBackoffMaxDelayInMsKey[];

// Finch parameter key for lambda in tile score decay calculation.
extern const char kTileScoreDecayLambdaKey[];

// Finch parameter key representing the minimum scores for new tiles that are in
// front of others.
extern const char kMinimumScoreForNewFrontTilesKey[];

// Finch parameter key for number of trending tiles to display.
extern const char kNumTrendingTilesKey[];

// Finch parameter key for max number of trending tile impressions.
extern const char kMaxTrendingTileImpressionsKey[];

// Finch parameter key for the starting position to shuffle unclicked tiles.
extern const char kTileShufflePositionKey[];

// Finch parameter key for number of non-interacted days to reset tile score.
extern const char kNumDaysToResetTileScore[];

class TileConfig {
 public:
  // Gets the URL for the Query Tiles service. If
  // |override_field_trial_param_value_if_empty| is false, server URL provided
  // by field trial param is preferred over |base_url|. Otherwise, |base_url| is
  // used. This method could return an empty URL if no valid URL is provided
  // though |base_url| or field trial param.
  static GURL GetQueryTilesServerUrl(
      const std::string& base_url,
      bool override_field_trial_param_value_if_empty);

  // Gets whether running the background task requires unmeter network
  // condition.
  static bool GetIsUnMeteredNetworkRequired();

  // Gets the experiment tag to be passed to server, given the country code.
  static std::string GetExperimentTag(const std::string& country_code);

  // Gets the maximum duration for holding current group's info and images.
  static base::TimeDelta GetExpireDuration();

  // Gets the image prefetch mode to determine how many images will be
  // prefetched in background task.
  static ImagePrefetchMode GetImagePrefetchMode();

  // Get the interval of schedule window in ms.
  static int GetScheduleIntervalInMs();

  // Get the maxmium window for randomization in ms.
  static int GetMaxRandomWindowInMs();

  // Get the schedule window duration from start to end in one-off task params
  // in ms.
  static int GetOneoffTaskWindowInMs();

  // Get the init delay (unit:ms) argument for backoff policy.
  static int GetBackoffPolicyArgsInitDelayInMs();

  // Get the max delay (unit:ms) argument for backoff policy.
  static int GetBackoffPolicyArgsMaxDelayInMs();

  // Get the lambda value used for calculating the tile score decay over time.
  static double GetTileScoreDecayLambda();

  // Get the minimum scrore for newly showing tiles that are in front of others.
  static double GetMinimumScoreForNewFrontTiles();

  // Get the number of trending tiles to be displayed at the same time.
  static int GetNumTrendingTilesToDisplay();

  // Get the maximum number of impressions for a trending tile to be displayed.
  static int GetMaxTrendingTileImpressions();

  // Get the starting position tp shuffle unclicked tiles. Tiles before this
  // position are not shuffled.
  static int GetTileShufflePosition();

  // Get the number of non-interacted days to reset tile score.
  static int GetNumDaysToResetTileScore();
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_
