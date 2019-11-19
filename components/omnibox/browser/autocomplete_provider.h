// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_H_

#include <stddef.h>

#include <map>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteInput;

typedef std::vector<metrics::OmniboxEventProto_ProviderInfo> ProvidersInfo;

// The AutocompleteProviders each return different kinds of matches,
// such as history or search matches.  These matches are given
// "relevance" scores.  Higher scores are better matches than lower
// scores.  The relevance scores and classes providing the respective
// matches are as listed below.
//
// IMPORTANT CAVEAT: The tables below are NOT COMPLETE.  Developers
// often forget to keep these tables in sync with the code when they
// change scoring algorithms or add new providers.  For example,
// neither the HistoryQuickProvider (which is a provider that appears
// often) nor the ShortcutsProvider are listed here.  For the best
// idea of how scoring works and what providers are affecting which
// queries, play with chrome://omnibox/ for a while.  While the tables
// below may have some utility, nothing compares with first-hand
// investigation and experience.
//
// ZERO SUGGEST (empty) input type:
// --------------------------------------------------------------------|-----
// Clipboard URL                                                       |  800
// Zero Suggest (most visited, Android only)                           |  600--
// Zero Suggest (default, may be overridden by server)                 |  100
// Local History Zero Suggest                                          |  500--
//
// UNKNOWN input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// HistoryURL (good exact or inline autocomplete matches, some inexact)| 1410++
// HistoryURL (intranet url never visited match, some inexact matches) | 1400++
// Search Primary Provider (past query in history within 2 days)       | 1399**
// Search Primary Provider (what you typed)                            | 1300
// HistoryURL (what you typed, some inexact matches)                   | 1200++
// Keyword (substituting, exact match)                                 | 1100
// Search Primary Provider (past query in history older than 2 days)   | 1050*
// HistoryURL (some inexact matches)                                   |  900++
// BookmarkProvider (prefix match in bookmark title or URL)            |  900+-
// Built-in                                                            |  860++
// Search Primary Provider (navigational suggestion)                   |  800++
// Search Primary Provider (suggestion)                                |  600++
// Keyword (inexact match)                                             |  450
// Search Secondary Provider (what you typed)                          |  250
// Search Secondary Provider (past query in history)                   |  200*
// Search Secondary Provider (navigational suggestion)                 |  150++
// Search Secondary Provider (suggestion)                              |  100++
// Non Personalized On Device Head Suggest Provider                    |    *
//                  (default value 99--, can be changed by Finch)
// Document Suggestions (*experimental): value controlled by Finch     |    *
//
// URL input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// HistoryURL (good exact or inline autocomplete matches, some inexact)| 1410++
// HistoryURL (intranet url never visited match, some inexact matches) | 1400++
// HistoryURL (what you typed, some inexact matches)                   | 1200++
// Keyword (substituting, exact match)                                 | 1100
// HistoryURL (some inexact matches)                                   |  900++
// Built-in                                                            |  860++
// Search Primary Provider (what you typed)                            |  850
// Search Primary Provider (navigational suggestion)                   |  800++
// Search Primary Provider (past query in history)                     |  750*
// Keyword (inexact match)                                             |  700
// Search Primary Provider (suggestion)                                |  300++
// Search Secondary Provider (what you typed)                          |  250
// Search Secondary Provider (past query in history)                   |  200*
// Search Secondary Provider (navigational suggestion)                 |  150++
// Search Secondary Provider (suggestion)                              |  100++
// Non Personalized On Device Head Suggest Provider                    |   99--
//
// QUERY input type:
// --------------------------------------------------------------------|-----
// Search Primary or Secondary (past query in history within 2 days)   | 1599**
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// Keyword (substituting, exact match)                                 | 1450
// Search Primary Provider (past query in history within 2 days)       | 1399**
// Search Primary Provider (what you typed)                            | 1300
// Search Primary Provider (past query in history older than 2 days)   | 1050*
// HistoryURL (inexact match)                                          |  900++
// BookmarkProvider (prefix match in bookmark title or URL)            |  900+-
// Search Primary Provider (navigational suggestion)                   |  800++
// Search Primary Provider (suggestion)                                |  600++
// Keyword (inexact match)                                             |  450
// Search Secondary Provider (what you typed)                          |  250
// Search Secondary Provider (past query in history)                   |  200*
// Search Secondary Provider (navigational suggestion)                 |  150++
// Search Secondary Provider (suggestion)                              |  100++
// Non Personalized On Device Head Suggest Provider                    |    *
//                  (default value 99--, can be changed by Finch)
//
// (A search keyword is a keyword with a replacement string; a bookmark keyword
// is a keyword with no replacement string, that is, a shortcut for a URL.)
//
// There are two possible providers for search suggestions. If the user has
// typed a keyword, then the primary provider is the keyword provider and the
// secondary provider is the default provider. If the user has not typed a
// keyword, then the primary provider corresponds to the default provider.
//
// Search providers may supply relevance values along with their results to be
// used in place of client-side calculated values.
//
// The value column gives the ranking returned from the various providers.
// ++: a series of matches with relevance from n up to (n + max_matches).
// --: a series of matches with relevance from n down to (n - max_matches).
// *:  relevance score falls off over time (discounted 50 points @ 15 minutes,
//     450 points @ two weeks)
// **: relevance score falls off over two days (discounted 99 points after two
//     days).
// +-: A base score that the provider will adjust upward or downward based on
//     provider-specific metrics.
//
// A single result provider for the autocomplete system.  Given user input, the
// provider decides what (if any) matches to return, their relevance, and their
// classifications.
class AutocompleteProvider
    : public base::RefCountedThreadSafe<AutocompleteProvider> {
 public:
  // Different AutocompleteProvider implementations.
  enum Type {
    TYPE_BOOKMARK = 1 << 0,
    TYPE_BUILTIN = 1 << 1,
    TYPE_HISTORY_QUICK = 1 << 2,
    TYPE_HISTORY_URL = 1 << 3,
    TYPE_KEYWORD = 1 << 4,
    TYPE_SEARCH = 1 << 5,
    TYPE_SHORTCUTS = 1 << 6,
    TYPE_ZERO_SUGGEST = 1 << 7,
    TYPE_CLIPBOARD = 1 << 8,
    TYPE_DOCUMENT = 1 << 9,
    TYPE_ON_DEVICE_HEAD = 1 << 10,
    TYPE_ZERO_SUGGEST_LOCAL_HISTORY = 1 << 11,
  };

  explicit AutocompleteProvider(Type type);

  // Returns a string describing a particular AutocompleteProvider type.
  static const char* TypeToString(Type type);

  // Called to start an autocomplete query.  The provider is responsible for
  // tracking its matches for this query and whether it is done processing the
  // query.  When new matches are available or the provider finishes, it
  // calls the controller's OnProviderUpdate() method.  The controller can then
  // get the new matches using the provider's accessors.
  // Exception: Matches available immediately after starting the query (that
  // is, synchronously) do not cause any notifications to be sent.  The
  // controller is expected to check for these without prompting (since
  // otherwise, starting each provider running would result in a flurry of
  // notifications).
  //
  // Once Stop() has been called, usually no more notifications should be sent.
  // (See comments on Stop() below.)
  //
  // |minimal_changes| is an optimization that lets the provider do less work
  // when the |input|'s text hasn't changed.  See the body of
  // OmniboxPopupModel::StartAutocomplete().
  virtual void Start(const AutocompleteInput& input, bool minimal_changes) = 0;

  // Advises the provider to stop processing.  This may be called even if the
  // provider is already done.  If the provider caches any results, it should
  // clear the cache based on the value of |clear_cached_results|.  Normally,
  // once this is called, the provider should not send more notifications to
  // the controller.
  //
  // If |user_inactivity_timer| is true, Stop() is being called because it's
  // been a long time since the user started the current query, and returning
  // further asynchronous results would normally just be disruptive.  Most
  // providers should still stop processing in this case, but continuing is
  // legal if there's a good reason the user is likely to want even long-
  // delayed asynchronous results, e.g. the user has explicitly invoked a
  // keyword extension and the extension is still processing the request.
  virtual void Stop(bool clear_cached_results,
                    bool due_to_user_inactivity);

  // Returns the enum equivalent to the name of this provider.
  // TODO(derat): Make metrics use AutocompleteProvider::Type directly, or at
  // least move this method to the metrics directory.
  metrics::OmniboxEventProto_ProviderType AsOmniboxEventProviderType() const;

  // Called to delete a match and the backing data that produced it.  This
  // match should not appear again in this or future queries.  This can only be
  // called for matches the provider marks as deletable.  This should only be
  // called when no query is running.
  // NOTE: Do NOT call OnProviderUpdate() in this method, it is the
  // responsibility of the caller to do so after calling us.
  virtual void DeleteMatch(const AutocompleteMatch& match);

  // Called when an omnibox event log entry is generated.  This gives
  // a provider the opportunity to add diagnostic information to the
  // logs.  A provider is expected to append a single entry of whatever
  // information it wants to |provider_info|.
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const;

  // Called when a new omnibox session starts or the current session ends.
  // This gives the opportunity to reset the internal state, if any, associated
  // with the previous session.
  virtual void ResetSession();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  //
  // Note: Subclasses that override this method must call the base class
  // method and include the response in their estimate.
  virtual size_t EstimateMemoryUsage() const;

  // Returns a suggested upper bound for how many matches this provider should
  // return.
  size_t provider_max_matches() const { return provider_max_matches_; }

  // Returns the set of matches for the current query.
  const ACMatches& matches() const { return matches_; }

  // Returns whether the provider is done processing the query.
  bool done() const { return done_; }

  // Returns this provider's type.
  Type type() const { return type_; }

  // Returns a string describing this provider's type.
  const char* GetName() const;

  typedef std::multimap<base::char16, base::string16> WordMap;

  // Finds the matches for |find_text| in |text|, classifies those matches,
  // merges those classifications with |original_class|, and returns the merged
  // classifications.
  // If |text_is_search_query| is false, matches are classified as MATCH, and
  // non-matches are classified as NONE. Otherwise, if |text_is_search_query| is
  // true, matches are classified as NONE, and non-matches are classified as
  // MATCH. This is done to mimic the behavior of SearchProvider which decorates
  // matches according to the approach used by Google Suggest.
  // |find_text| and |text| will be lowercased.
  //
  //   For example, given
  //     |find_text| is "sp new",
  //     |text| is "Sports and News at sports.somesite.com - visit us!",
  //     |text_is_search_query| is false, and
  //     |original_class| is {{0, NONE}, {19, URL}, {38, NONE}} (marking
  //     "sports.somesite.com" as a URL),
  //   Then this will return
  //     {{0, MATCH}, {2, NONE}, {11, MATCH}, {14, NONE}, {19, URL|MATCH},
  //     {21, URL}, {38, NONE}}; i.e.,
  //     "Sports and News at sports.somesite.com - visit us!"
  //      ^ ^        ^  ^    ^ ^                ^
  //      0 2        11 14  19 21               38
  //      M N        M  N  U|M U                N
  //
  //   For example, given
  //     |find_text| is "canal",
  //     |text| is "panama canal",
  //     |text_is_search_query| is true, and
  //     |original_class| is {{0, NONE}},
  //   Then this will return
  //     {{0,MATCH}, {7, NONE}}; i.e.,
  //     "panama canal"
  //      ^      ^
  //      0 M    7 N
  static ACMatchClassifications ClassifyAllMatchesInString(
      const base::string16& find_text,
      const base::string16& text,
      const bool text_is_search_query,
      const ACMatchClassifications& original_class = ACMatchClassifications());

  // Used to determine if we're in keyword mode, if experimental keyword
  // mode is enabled, and if we're confident that the user is intentionally
  // (not accidentally) in keyword mode. Combined, this method returns
  // whether the caller should perform steps that are only valid in this state.
  static bool InExplicitExperimentalKeywordMode(const AutocompleteInput& input,
                                                const base::string16& keyword);

  // Uses the keyword entry mode in |input| (and possibly compare the length
  // of the user input vs |keyword|) to decide if the user intentionally
  // entered keyword mode.
  static bool IsExplicitlyInKeywordMode(const AutocompleteInput& input,
                                        const base::string16& keyword);

 protected:
  friend class base::RefCountedThreadSafe<AutocompleteProvider>;
  FRIEND_TEST_ALL_PREFIXES(BookmarkProviderTest, InlineAutocompletion);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest,
                           DemoteOnDeviceSearchSuggestions);

  typedef std::pair<bool, base::string16> FixupReturn;

  virtual ~AutocompleteProvider();

  // Fixes up user URL input to make it more possible to match against.  Among
  // many other things, this takes care of the following:
  // * Prepending file:// to file URLs
  // * Converting drive letters in file URLs to uppercase
  // * Converting case-insensitive parts of URLs (like the scheme and domain)
  //   to lowercase
  // * Convert spaces to %20s
  // Note that we don't do this in AutocompleteInput's constructor, because if
  // e.g. we convert a Unicode hostname to punycode, other providers will show
  // output that surprises the user ("Search Google for xn--6ca.com").
  // Returns a bool indicating whether fixup succeeded, as well as the fixed-up
  // input text.  The returned string will be the same as the input string if
  // fixup failed; this lets callers who don't care about failure simply use the
  // string unconditionally.
  static FixupReturn FixupUserInput(const AutocompleteInput& input);

  // Trims "http:" and up to two subsequent slashes from |url|.  Returns the
  // number of characters that were trimmed.
  // NOTE: For a view-source: URL, this will trim from after "view-source:" and
  // return 0.
  static size_t TrimHttpPrefix(base::string16* url);

  const size_t provider_max_matches_;

  ACMatches matches_;
  bool done_;

  Type type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompleteProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_H_
