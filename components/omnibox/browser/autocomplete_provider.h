// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_H_

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteInput;
class AutocompleteProviderListener;

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
// ZERO SUGGEST (empty input type) on NTP:
// --------------------------------------------------------------------|-----
// Query Tiles (Android only)                                          |  1599
// Clipboard (Mobile only)                                             |  1501
// Remote Zero Suggest (relevance expected to be overridden by server) |  100
// Local History Zero Suggest (signed-out users)                       |  1450--
// Local History Zero Suggest (signed-in users)                        |  500--
//
// ZERO SUGGEST (empty input type) on SERP:
// --------------------------------------------------------------------|-----
// Verbatim Match (Mobile only)                                        |  1600
// Clipboard (Mobile only)                                             |  1501
//
// ZERO SUGGEST (empty input type) on OTHER (e.g., contextual web):
// --------------------------------------------------------------------|-----
// Verbatim Match (Mobile only)                                        |  1600
// Clipboard (Mobile only)                                             |  1501
// Most Visited Carousel (Android only)                                |  1500
// Most Visited Sites (Mobile only)                                    |  600--
// Remote Zero Suggest (relevance expected to be overridden by server) |  100
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
    TYPE_QUERY_TILE = 1 << 12,
    TYPE_MOST_VISITED_SITES = 1 << 13,
    TYPE_VERBATIM_MATCH = 1 << 14,
    TYPE_VOICE_SUGGEST = 1 << 15,
    TYPE_HISTORY_FUZZY = 1 << 16,
    TYPE_OPEN_TAB = 1 << 17,
    TYPE_HISTORY_CLUSTER_PROVIDER = 1 << 18,
    TYPE_CALCULATOR = 1 << 19,
    TYPE_FEATURED_SEARCH = 1 << 20,
    TYPE_HISTORY_EMBEDDINGS = 1 << 21,
    // When adding a value here, also update:
    // - omnibox_event.proto
    // - `AutocompleteProvider::AsOmniboxEventProviderType`
    // - `AutocompleteProvider::TypeToString`
    // - `AutocompleteClassifier::DefaultOmniboxProviders`
  };

  explicit AutocompleteProvider(Type type);

  AutocompleteProvider(const AutocompleteProvider&) = delete;
  AutocompleteProvider& operator=(const AutocompleteProvider&) = delete;

  // Returns a string describing a particular AutocompleteProvider type.
  static const char* TypeToString(Type type);

  // Used to communicate async matches to consumers (usually the
  // `AutocompleteController`). Consumers invoke `AddListener()` to register
  // their interest, while child `AutocompleteProvider` implementations invoke
  // `NotifyListeners().`
  void AddListener(AutocompleteProviderListener* listener);
  void NotifyListeners(bool updated_matches) const;

  // Called on page load. Used start a prefetch request to warm up the
  // provider's underlying service(s) and/or optionally cache the provider's
  // otherwise async response. A prefetch request must conform to the following:
  // - It must be posted on a sequence to minimize contention on page load.
  // - It must *not* depend on or affect the provider's state.
  // - It must *not* stop the provider.
  // - It need *not* stop when the provider is stopped.
  // - It must *not* call NotifyListeners() after completing a prefetch request.
  // - It must make prefetched response accessible to other instances of the
  //   provider, e.g., via user prefs or a keyed service, if applicable.
  // The default implementation DCHECKs whether async requests are allowed.
  // Overridden functions must call `AutocompleteProvider::StartPrefetch()` with
  // the same arguments passed to the function.
  virtual void StartPrefetch(const AutocompleteInput& input);

  // Called to start an autocomplete query.  The provider is responsible for
  // tracking its matches for this query and whether it is done processing the
  // query.  When new matches are available or the provider finishes, it
  // calls NotifyListeners() which calls the controller's OnProviderUpdate()
  // method.  The controller can then get the new matches using the provider's
  // accessors. Exception: Matches available immediately after starting the
  // query (that is, synchronously) do not cause any notifications to be sent.
  // The controller is expected to check for these without prompting (since
  // otherwise, starting each provider running would result in a flurry of
  // notifications).
  //
  // Providers should invalidate any in-progress requests and make sure *not* to
  // call NotifyListeners() method for invalidated requests by calling Stop().
  // Once Stop() has been called, usually no more notifications should be sent.
  // (See comments on Stop() below.)
  //
  // |minimal_changes| is an optimization that lets the provider do less work
  // when the |input|'s text hasn't changed.  See the body of
  // AutocompleteController::Start().
  virtual void Start(const AutocompleteInput& input, bool minimal_changes) = 0;

  // Advises the provider to stop processing.  This may be called even if the
  // provider is already done.  If the provider caches any results, it should
  // clear the cache based on the value of `clear_cached_results`.  Normally,
  // once this is called, the provider should not send more notifications to
  // the controller.
  //
  // If `user_inactivity_timer` is true, Stop() is being called because it's
  // been a long time since the user started the current query, and returning
  // further asynchronous results would normally just be disruptive.  Most
  // providers should still stop processing in this case, but continuing is
  // legal if there's a good reason the user is likely to want even long-
  // delayed asynchronous results, e.g. the user has explicitly invoked a
  // keyword extension and the extension is still processing the request.
  //
  // The default implementation sets `done_` to true and clears `matches_` if
  // `clear_cached_results` is true. Overridden functions must call
  // `AutocompleteProvider::Stop()` with the same arguments passed to the
  // function.
  virtual void Stop(bool clear_cached_results, bool due_to_user_inactivity);

  // Returns the enum equivalent to the name of this provider.
  // TODO(derat): Make metrics use AutocompleteProvider::Type directly, or at
  // least move this method to the metrics directory.
  metrics::OmniboxEventProto_ProviderType AsOmniboxEventProviderType() const;

  // Called to delete a match and the backing data that produced it.  This
  // match should not appear again in this or future queries.  This can only be
  // called for matches the provider marks as deletable.  This should only be
  // called when no query is running.
  // NOTE: Do NOT call NotifyListeners() in this method, it is the
  // responsibility of the caller to do so after calling us.
  virtual void DeleteMatch(const AutocompleteMatch& match);

  // Called to delete an element of a match. This element should not appear
  // again in this or future queries. Unlike DeleteMatch, this call does not
  // delete the entire AutocompleteMatch, but focuses on just one part of it.
  // NOTE: Do NOT call NotifyListeners() in this method, it is the
  // responsibility of the caller to do so after calling us.
  virtual void DeleteMatchElement(const AutocompleteMatch& match,
                                  size_t element_index);

  // Called when an omnibox event log entry is generated.  This gives
  // a provider the opportunity to add diagnostic information to the
  // logs.  A provider is expected to append a single entry of whatever
  // information it wants to |provider_info|.
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  //
  // Note: Subclasses that override this method must call the base class
  // method and include the response in their estimate.
  virtual size_t EstimateMemoryUsage() const;

  // Returns a map of suggestion group IDs to suggestion group information
  // corresponding to |matches_|.
  const omnibox::GroupConfigMap& suggestion_groups_map() const {
    return suggestion_groups_map_;
  }

  // Returns a suggested upper bound for how many matches this provider should
  // return.
  size_t provider_max_matches() const { return provider_max_matches_; }

  // Returns a suggested upper bound for how many matches this provider should
  // return while in keyword mode.
  size_t provider_max_matches_in_keyword_mode() const {
    return provider_max_matches_in_keyword_mode_;
  }

  // Returns the set of matches for the current query.
  const ACMatches& matches() const { return matches_; }

  // Returns whether the provider is done processing the last `Start()` request.
  // Should not be set true for `StartPrefetch()` requests in order to remain
  // consistent with `AutocompleteController::done()`; i.e., if `done_` is false
  // for any provider, then the `AutocompleteController::done_` must also be
  // false. This ensures the controller can determine when each provider
  // finishes processing async requests. Should be true after either `Stop()` or
  // `Start()` with `AutocompleteInput.omit_asynchronous_matches_` set to true
  // are called.
  bool done() const { return done_; }

  // Returns this provider's type.
  Type type() const { return type_; }

  // Returns a string describing this provider's type.
  const char* GetName() const;

  typedef std::multimap<char16_t, std::u16string> WordMap;

  // Trims "http:" or "https:" and up to two subsequent slashes from |url|. If
  // |trim_https| is true, trims "https:", otherwise trims "http:". Returns the
  // number of characters that were trimmed.
  // NOTE: For a view-source: URL, this will trim from after "view-source:" and
  // return 0.
  static size_t TrimSchemePrefix(std::u16string* url, bool trim_https);

 protected:
  friend class base::RefCountedThreadSafe<AutocompleteProvider>;
  friend class FakeAutocompleteProvider;
  FRIEND_TEST_ALL_PREFIXES(BookmarkProviderTest, InlineAutocompletion);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest,
                           DemoteOnDeviceSearchSuggestions);

  typedef std::pair<bool, std::u16string> FixupReturn;

  virtual ~AutocompleteProvider();

  // Limits the size of `matches_` to `max_matches`. When ML scoring is enabled,
  // the provider should pass all suggestions to the controller. In that case,
  // this does not resize the list of matches, but instead marks all matches
  // beyond `max_matches` as zero relevance and `culled_by_provider`.
  void ResizeMatches(size_t max_matches, bool ml_scoring_enabled);

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

  std::vector<raw_ptr<AutocompleteProviderListener, VectorExperimental>>
      listeners_;

  const size_t provider_max_matches_;
  const size_t provider_max_matches_in_keyword_mode_{7};

  ACMatches matches_;
  // A map of suggestion group IDs to suggestion group information.
  // `omnibox::BuildDefaultGroups()` will generate static groups. Providers can
  // set this to create dynamic groups; e.g. the `ZeroSuggestProvider` does this
  // based on groups received from the server.
  omnibox::GroupConfigMap suggestion_groups_map_{};
  bool done_{true};

  Type type_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_H_
