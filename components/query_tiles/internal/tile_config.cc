// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_config.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "components/query_tiles/switches.h"

namespace query_tiles {

// Default base URL string for the Query Tiles server.
constexpr char kDefaultBaseURL[] = "https://chromeupboarding-pa.googleapis.com";

// Default URL string for GetQueryTiles RPC.
constexpr char kDefaultGetQueryTilePath[] = "/v1/querytiles";

// Finch parameter key for experiment tag to be passed to the server.
constexpr char kExperimentTagKey[] = "experiment_tag";

// Finch parameter key for base server URL to retrieve the tiles.
constexpr char kBaseURLKey[] = "base_url";

// Finch parameter key for expire duration in seconds.
constexpr char kExpireDurationKey[] = "expire_duration";

// Finch parameter key for expire duration in seconds.
constexpr char kIsUnmeteredNetworkRequiredKey[] =
    "is_unmetered_network_required";

// Finch parameter key for schedule interval.
constexpr char kScheduleIntervalKey[] =
    "tile_background_task_schedule_interval";

// Finch parameter key for random window.
constexpr char kMaxRandomWindowKey[] = "tile_background_task_random_window";

// Finch parameter key for oneoff task window.
constexpr char kOneoffTaskWindowKey[] =
    "tile_background_task_oneoff_task_window";

const char kImagePrefetchModeKey[] = "image_prefetch_mode";

// Finch parameter key for Backoff policy initial delay in ms.
constexpr char kBackoffInitDelayInMsKey[] = "backoff_policy_init_delay_in_ms";

// Finch parameter key for Backoff policy maximum delay in ms.
constexpr char kBackoffMaxDelayInMsKey[] = "backoff_policy_max_delay_in_ms";

constexpr char kTileScoreDecayLambdaKey[] = "tile_score_decay_lambda";

constexpr char kMinimumScoreForNewFrontTilesKey[] =
    "min_score_for_new_front_tiles";

// Default expire duration.
constexpr int kDefaultExpireDurationInSeconds = 48 * 60 * 60;  // 2 days.

// Default periodic interval of background task.
constexpr int kDefaultScheduleInterval = 12 * 3600 * 1000;  // 12 hours.

// Default length of random window added to the interval.
constexpr int kDefaultRandomWindow = 4 * 3600 * 1000;  // 4 hours.

// Default delta value from start window time to end window time in one-off
// background task.
constexpr int kDefaultOneoffTaskWindow = 2 * 3600 * 1000;  // 2 hours.

// Default initial delay in backoff policy.
constexpr int kDefaultBackoffInitDelayInMs = 30 * 1000;  // 30 seconds.

// Default maximum delay in backoff policy, also used for suspend duration.
constexpr int kDefaultBackoffMaxDelayInMs = 24 * 3600 * 1000;  // 1 day.

// Default lambda value used for calculating tile score decay over time.
constexpr double kDefaultTileScoreDecayLambda = -0.099;

// Default minimum score for new tiles in front of others. 0.9 is chosen so
// that new tiles will have a higher score than tiles that have not been
// clicked for 2 days.
constexpr double kDefaultMinimumTileScoreForNewFrontTiles = 0.9;

namespace {

// For testing. Json string for single tier experiment tag.
const char kQueryTilesSingleTierExperimentTag[] = "{\"maxLevels\": \"1\"}";

// Json Experiment tag for enabling trending queries.
const char kQueryTilesEnableTrendingExperimentTag[] =
    "{\"enableTrending\": \"true\"}";

const GURL BuildGetQueryTileURL(const GURL& base_url, const char* path) {
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return base_url.ReplaceComponents(replacements);
}
}  // namespace

// static
GURL TileConfig::GetQueryTilesServerUrl() {
  return GetQueryTilesServerUrl(base::GetFieldTrialParamValueByFeature(
      features::kQueryTiles, kBaseURLKey));
}

// static
GURL TileConfig::GetQueryTilesServerUrl(const std::string& base_url) {
  GURL server_url = base_url.empty() ? GURL(kDefaultBaseURL) : GURL(base_url);
  return BuildGetQueryTileURL(server_url, kDefaultGetQueryTilePath);
}

// static
bool TileConfig::GetIsUnMeteredNetworkRequired() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQueryTiles, kIsUnmeteredNetworkRequiredKey, false);
}

// static
std::string TileConfig::GetExperimentTag() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kQueryTilesSingleTier)) {
    return kQueryTilesSingleTierExperimentTag;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kQueryTilesEnableTrending)) {
    return kQueryTilesEnableTrendingExperimentTag;
  }

  return base::GetFieldTrialParamValueByFeature(features::kQueryTiles,
                                                kExperimentTagKey);
}

// static
base::TimeDelta TileConfig::GetExpireDuration() {
  int time_in_seconds = base::GetFieldTrialParamByFeatureAsInt(
      features::kQueryTiles, kExpireDurationKey,
      kDefaultExpireDurationInSeconds);
  return base::TimeDelta::FromSeconds(time_in_seconds);
}

// static
ImagePrefetchMode TileConfig::GetImagePrefetchMode() {
  std::string image_prefetch_mode = base::GetFieldTrialParamValueByFeature(
      features::kQueryTiles, kImagePrefetchModeKey);
  if (image_prefetch_mode == "none")
    return ImagePrefetchMode::kNone;
  if (image_prefetch_mode == "top")
    return ImagePrefetchMode::kTopLevel;
  if (image_prefetch_mode == "all")
    return ImagePrefetchMode::kAll;

  return ImagePrefetchMode::kTopLevel;
}

// static
int TileConfig::GetScheduleIntervalInMs() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kQueryTiles, kScheduleIntervalKey, kDefaultScheduleInterval);
}

// static
int TileConfig::GetMaxRandomWindowInMs() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kQueryTiles, kMaxRandomWindowKey, kDefaultRandomWindow);
}

// static
int TileConfig::GetOneoffTaskWindowInMs() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kQueryTiles, kOneoffTaskWindowKey, kDefaultOneoffTaskWindow);
}

// static
int TileConfig::GetBackoffPolicyArgsInitDelayInMs() {
  return base::GetFieldTrialParamByFeatureAsInt(features::kQueryTiles,
                                                kBackoffInitDelayInMsKey,
                                                kDefaultBackoffInitDelayInMs);
}

// static
int TileConfig::GetBackoffPolicyArgsMaxDelayInMs() {
  return base::GetFieldTrialParamByFeatureAsInt(features::kQueryTiles,
                                                kBackoffMaxDelayInMsKey,
                                                kDefaultBackoffMaxDelayInMs);
}

// static
double TileConfig::GetTileScoreDecayLambda() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      features::kQueryTiles, kTileScoreDecayLambdaKey,
      kDefaultTileScoreDecayLambda);
}

// static
double TileConfig::GetMinimumScoreForNewFrontTiles() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      features::kQueryTiles, kMinimumScoreForNewFrontTilesKey,
      kDefaultMinimumTileScoreForNewFrontTiles);
}

}  // namespace query_tiles
