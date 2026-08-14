// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_MOST_VISITED_METRICS_LOGGER_H_
#define CHROME_BROWSER_UI_SEARCH_MOST_VISITED_METRICS_LOGGER_H_

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/ntp_tile_impression.h"

// Interface and base implementation for logging metrics and events from Most
// Visited Tiles across different surfaces (NTP, 3P NTP, Loomnibox, etc.).
class MostVisitedMetricsLogger {
 public:
  explicit MostVisitedMetricsLogger(std::string_view histogram_prefix);
  virtual ~MostVisitedMetricsLogger();

  // Called when a most visited tile event occurs.
  virtual void LogEvent(NTPLoggingEventType event, base::TimeDelta time);

  // Called when all most visited tiles have finished loading.
  virtual void LogMostVisitedLoaded(base::TimeDelta time,
                                    bool using_most_visited,
                                    bool using_custom_links,
                                    bool using_enterprise_shortcuts,
                                    bool is_visible,
                                    std::optional<bool> is_expanded);

  // Logs an impression on one of the tiles.
  virtual void LogMostVisitedImpression(
      const ntp_tiles::NTPTileImpression& impression);

  // Logs a navigation on one of the tiles.
  virtual void LogMostVisitedNavigation(
      const ntp_tiles::NTPTileImpression& impression);

 private:
  std::string histogram_prefix_;
  bool has_recorded_impressions_ = false;
  std::array<std::optional<ntp_tiles::NTPTileImpression>,
             ntp_tiles::kMaxNumTiles>
      logged_impressions_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_MOST_VISITED_METRICS_LOGGER_H_
