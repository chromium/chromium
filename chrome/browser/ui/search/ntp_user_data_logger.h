// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_LOGGER_H_
#define CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_LOGGER_H_

#include <stddef.h>

#include <array>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if defined(OS_ANDROID)
#error "Instant is only used on desktop";
#endif

namespace content {
class WebContents;
}

// Helper class for logging data from the NTP. Attached to each NTP instance.
class NTPUserDataLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NTPUserDataLogger> {
 public:
  ~NTPUserDataLogger() override;

  // Gets the associated NTPUserDataLogger, creating it if necessary.
  //
  // MUST be called only when the NTP is active.
  static NTPUserDataLogger* GetOrCreateFromWebContents(
      content::WebContents* content);

  // Called when an event occurs on the NTP that requires a counter to be
  // incremented. |time| is the delta time from navigation start until this
  // event happened. The NTP_ALL_TILES_LOADED event may be logged from all NTPs;
  // all others require Google as the default search provider.
  void LogEvent(NTPLoggingEventType event, base::TimeDelta time);

  // Called when a search suggestion event occurs on the NTP that has an integer
  // value associated with it; N suggestions were shown on this NTP load, the
  // Nth suggestion was clicked, etc. |time| is the delta time from navigation
  // start until this event happened. Requires Google as the default search
  // provider.
  void LogSuggestionEventWithValue(NTPSuggestionsLoggingEventType event,
                                   int data,
                                   base::TimeDelta time);

  // Logs an impression on one of the NTP tiles by given details.
  void LogMostVisitedImpression(const ntp_tiles::NTPTileImpression& impression);

  // Logs a navigation on one of the NTP tiles by a given impression.
  void LogMostVisitedNavigation(const ntp_tiles::NTPTileImpression& impression);

 protected:
  explicit NTPUserDataLogger(content::WebContents* contents);

  void set_ntp_url_for_testing(const GURL& ntp_url) { ntp_url_ = ntp_url; }

 private:
  friend class content::WebContentsUserData<NTPUserDataLogger>;

  FRIEND_TEST_ALL_PREFIXES(NTPUserDataLoggerTest, ShouldRecordLoadTime);
  FRIEND_TEST_ALL_PREFIXES(NTPUserDataLoggerTest, ShouldRecordNumberOfTiles);
  FRIEND_TEST_ALL_PREFIXES(NTPUserDataLoggerTest,
                           ShouldNotRecordImpressionsForBinsBeyondMax);
  FRIEND_TEST_ALL_PREFIXES(NTPUserDataLoggerTest,
                           ShouldRecordImpressionsAgainAfterNavigating);

  // content::WebContentsObserver override
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // Implementation of NavigationEntryCommitted; separate for test.
  void NavigatedFromURLToURL(const GURL& from, const GURL& to);

  // Returns whether Google is selected as the default search engine. Virtual
  // for testing.
  virtual bool DefaultSearchProviderIsGoogle() const;

  // Returns whether a custom background is configured. Virtual for testing.
  virtual bool CustomBackgroundIsConfigured() const;

  // Returns whether the user has customized their shortcuts. Will always be
  // false if Most Visited shortcuts are enabled. Virtual for testing.
  virtual bool AreShortcutsCustomized() const;

  // Returns the current user shortcut settings. Virtual for testing.
  virtual std::pair<bool, bool> GetCurrentShortcutSettings() const;

  // Logs a number of statistics regarding the NTP. Called when an NTP tab is
  // about to be deactivated (be it by switching tabs, losing focus or closing
  // the tab/shutting down Chrome), or when the user navigates to a URL.
  void EmitNtpStatistics(base::TimeDelta load_time);

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
  std::array<base::Optional<ntp_tiles::NTPTileImpression>,
             ntp_tiles::kMaxNumTiles>
      logged_impressions_;

  // Whether we have already emitted NTP stats for this web contents.
  bool has_emitted_;

  bool should_record_doodle_load_time_;

  // Are stats being logged during Chrome startup?
  bool during_startup_;

  // The URL of this New Tab Page - varies based on NTP version.
  GURL ntp_url_;

  // The profile in which this New Tab Page was loaded.
  Profile* profile_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(NTPUserDataLogger);
};

#endif  // CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_LOGGER_H_
