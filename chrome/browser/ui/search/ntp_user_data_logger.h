// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_LOGGER_H_
#define CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_LOGGER_H_

#include <stddef.h>

#include <array>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/ntp_tile_impression.h"

#if BUILDFLAG(IS_ANDROID)
#error "Instant is only used on desktop";
#endif

// Helper class for logging data from the NTP. Attached to each NTP instance.
class NTPUserDataLogger {
 public:
  // Creates a NTPUserDataLogger. MUST be called only when the NTP is active.
  NTPUserDataLogger(Profile* profile,
                    const GURL& ntp_url,
                    base::Time ntp_navigation_start_time);

  NTPUserDataLogger(const NTPUserDataLogger&) = delete;
  NTPUserDataLogger& operator=(const NTPUserDataLogger&) = delete;

  virtual ~NTPUserDataLogger();

  // Called when a One Google Bar fetch has been completed after |duration|.
  // |success| is true if the fetch was successful.
  static void LogOneGoogleBarFetchDuration(bool success,
                                           const base::TimeDelta& duration);

  // Called when an event occurs on the NTP that requires a counter to be
  // incremented. |time| is the delta time from navigation start until this
  // event happened. The NTP_ALL_TILES_LOADED event may be logged from all NTPs;
  // all others require Google as the default search provider.
  void LogEvent(NTPLoggingEventType event, base::TimeDelta time);

  // Called when all NTP tiles have finished loading (successfully or failing).
  void LogMostVisitedLoaded(base::TimeDelta time,
                            bool using_most_visited,
                            bool is_visible);

  // Logs an impression on one of the NTP tiles by given details.
  void LogMostVisitedImpression(const ntp_tiles::NTPTileImpression& impression);

  // Logs a navigation on one of the NTP tiles by a given impression.
  void LogMostVisitedNavigation(const ntp_tiles::NTPTileImpression& impression);

 private:
  // Returns whether Google is selected as the default search engine. Virtual
  // for testing.
  virtual bool DefaultSearchProviderIsGoogle() const;

  // Returns whether a custom background is configured. Virtual for testing.
  virtual bool CustomBackgroundIsConfigured() const;

  // Logs a number of statistics regarding the NTP. Called when an NTP tab is
  // about to be deactivated (be it by switching tabs, losing focus or closing
  // the tab/shutting down Chrome), or when the user navigates to a URL.
  void EmitNtpStatistics(base::TimeDelta load_time,
                         bool using_most_visited,
                         bool is_visible);

  void EmitNtpTraceEvent(const char* event_name, base::TimeDelta duration);

  void RecordDoodleImpression(base::TimeDelta time,
                              bool is_cta,
                              bool from_cache);

  // Logs the user |action| via base::RecordAction.
  void RecordAction(const char* action);

  // Records whether we have yet logged an impression for the tile at a given
  // index and if so the corresponding details. A typical NTP will log 9
  // impressions, but could record fewer for new users that haven't built up a
  // history yet. If the user has customized their shortcuts, this number can
  // increase up to 10 impressions.
  //
  // If something happens that causes the NTP to pull tiles from different
  // sources, such as signing in (switching from client to server tiles), then
  // only the impressions for the first source will be logged, leaving the
  // number of impressions for a source slightly out-of-sync with navigations.
  std::array<std::optional<ntp_tiles::NTPTileImpression>,
             ntp_tiles::kMaxNumTiles>
      logged_impressions_;

  // Whether we have already emitted NTP stats for this web contents.
  bool has_emitted_ = false;

  bool should_record_doodle_load_time_ = true;

  // Are stats being logged during Chrome startup?
  bool during_startup_;

  // The URL of this New Tab Page - varies based on NTP version.
  GURL ntp_url_;

  // The profile in which this New Tab Page was loaded.
  raw_ptr<Profile> profile_;

  // Keeps the starting time of NTP navigation.
  const base::TimeTicks ntp_navigation_start_time_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_LOGGER_H_
