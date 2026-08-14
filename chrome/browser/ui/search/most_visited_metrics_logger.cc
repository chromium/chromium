// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/most_visited_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/metrics.h"

MostVisitedMetricsLogger::MostVisitedMetricsLogger(
    std::string_view histogram_prefix)
    : histogram_prefix_(histogram_prefix) {
  CHECK(!histogram_prefix_.empty());
}

MostVisitedMetricsLogger::~MostVisitedMetricsLogger() = default;

void MostVisitedMetricsLogger::LogEvent(NTPLoggingEventType /*event*/,
                                        base::TimeDelta /*time*/) {}

void MostVisitedMetricsLogger::LogMostVisitedImpression(
    const ntp_tiles::NTPTileImpression& impression) {
  if (impression.index >= ntp_tiles::kMaxNumTiles ||
      logged_impressions_[impression.index].has_value()) {
    return;
  }
  logged_impressions_[impression.index] = impression;
}

void MostVisitedMetricsLogger::LogMostVisitedLoaded(
    base::TimeDelta /*time*/,
    bool /*using_most_visited*/,
    bool /*using_custom_links*/,
    bool /*using_enterprise_shortcuts*/,
    bool /*is_visible*/,
    std::optional<bool> is_expanded) {
  if (has_recorded_impressions_) {
    return;
  }

  int tiles_count = 0;
  for (const std::optional<ntp_tiles::NTPTileImpression>& impression :
       logged_impressions_) {
    if (!impression.has_value()) {
      break;
    }
    ntp_tiles::metrics::RecordTileImpression(*impression, histogram_prefix_);
    ++tiles_count;
  }

  ntp_tiles::metrics::RecordPageImpression(tiles_count, histogram_prefix_);

  if (is_expanded.has_value()) {
    base::UmaHistogramBoolean(
        base::StrCat({histogram_prefix_, ".MostVisited.IsExpandedOnLoad"}),
        is_expanded.value());
  }

  has_recorded_impressions_ = true;
}

void MostVisitedMetricsLogger::LogMostVisitedNavigation(
    const ntp_tiles::NTPTileImpression& impression) {
  ntp_tiles::metrics::RecordTileClick(impression, histogram_prefix_);
  base::RecordAction(base::UserMetricsAction("MostVisited_Clicked"));
}
