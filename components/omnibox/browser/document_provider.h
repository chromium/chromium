// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the document autocomplete provider. This experimental
// provider uses an experimental API with keys and endpoint provided at
// developer build-time, so it is feature-flagged off by default.

#ifndef COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/mru_cache.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "components/omnibox/browser/search_provider.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteProviderListener;
class AutocompleteProviderClient;

namespace base {
class Value;
}

namespace network {
class SimpleURLLoader;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Autocomplete provider for personalized documents owned or readable by the
// signed-in user. In practice this is a second request in parallel with that
// to the default search provider.
class DocumentProvider : public AutocompleteProvider {
 public:
  // Creates and returns an instance of this provider.
  static DocumentProvider* Create(AutocompleteProviderClient* client,
                                  AutocompleteProviderListener* listener,
                                  size_t cache_size = 20);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;
  void ResetSession() override;

  // Registers a client-side preference to enable document suggestions.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns a set of classifications that highlight all the occurrences of
  // |input_text| at word breaks in |text|. E.g., given |input_text|
  // "rain if you dare" and |text| "how to tell if your kitten is a rainbow",
  // will return the classifications:
  //             __ ___              ____
  // how to tell if your kitten is a rainbow
  // ^           ^ ^^   ^            ^  ^
  // NONE        M |M   |            |  NONE
  //               NONE NONE         MATCH
  static ACMatchClassifications Classify(const base::string16& input_text,
                                         const base::string16& text);

  // Builds a GURL to use for deduping against other document/history
  // suggestions. Multiple URLs may refer to the same document.
  // Returns an empty GURL if not a recognized Docs URL.
  // The URL returned is not guaranteed to be navigable and should only be used
  // as a deduping token.
  static const GURL GetURLForDeduping(const GURL& url);

 private:
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, CheckFeatureBehindFlag);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteNoIncognito);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteNoSync);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteClientSettingOff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteDefaultSearch);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeatureNotInExplicitKeywordMode);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteServerBackoff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, IsInputLikelyURL);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, ParseDocumentSearchResults);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ProductDescriptionStringsAndAccessibleLabels);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsBreakTies);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsBreakTiesCascade);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsBreakTiesZeroLimit);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsWithBackoff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsWithIneligibleFlag);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, GenerateLastModifiedString);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, Scoring);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, Caching);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, MinQueryLength);

  using MatchesCache = base::MRUCache<GURL, AutocompleteMatch>;

  DocumentProvider(AutocompleteProviderClient* client,
                   AutocompleteProviderListener* listener,
                   size_t cache_size);

  ~DocumentProvider() override;

  // Determines whether the profile/session/window meet the feature
  // prerequisites.
  bool IsDocumentProviderAllowed(AutocompleteProviderClient* client,
                                 const AutocompleteInput& input);

  // Determines if the input is a URL, or is the start of the user entering one.
  // We avoid queries for these cases for quality and scaling reasons.
  static bool IsInputLikelyURL(const AutocompleteInput& input);

  // Called by |debouncer_|, queued when |start| is called.
  void Run();

  // Called when the network request for suggestions has completed.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);

  // The function updates |matches_| with data parsed from |json_data|.
  // The update is not performed if |json_data| is invalid.
  // Returns whether |matches_| changed.
  bool UpdateResults(const std::string& json_data);

  // Callback for when the loader is available with a valid token.
  void OnDocumentSuggestionsLoaderAvailable(
      std::unique_ptr<network::SimpleURLLoader> loader);

  // Parses document search result JSON.
  // Returns true if |matches| was populated with fresh suggestions.
  ACMatches ParseDocumentSearchResults(const base::Value& root_val);

  // Appends |matches_cache_| to |matches_|. Updates their classifications
  // according to |input_.text()| and sets their relevance to 0.
  // |skip_n_most_recent_matches| indicates the number of cached matches already
  // in |matches_|. E.g. if the drive server responded with 3 docs, these 3 docs
  // are added both to |matches_| and |matches_cache| prior to invoking
  // |AddCachedMatches| in order to avoid duplicate matches.
  void CopyCachedMatchesToMatches(size_t skip_n_most_recent_matches = 0);

  // Generates the localized last-modified timestamp to present to the user.
  // Full date for old files, mm/dd within the same calendar year, or time-of-
  // day if a file was modified on the same date.
  // |now| should generally be base::Time::Now() but is passed in for testing.
  static base::string16 GenerateLastModifiedString(
      const std::string& modified_timestamp_string,
      base::Time now);

  // Don't request doc suggestions for inputs shorter than |min_query_length_|
  // or longer than |max_query_length_|. A value of -1 indicates no limit. These
  // help limit the load on backend servers.
  const size_t min_query_length_;
  const size_t max_query_length_;
  // Hide doc suggestions for inputs shorter than |min_query_show_length_| or
  // longer than |max_query_show_length_|. A value of -1 indicates no limit.
  // These help analyze experiments by allowing observing the effect of changing
  // |min(max)_query_length_| while keeping data populations consistent.
  const size_t min_query_show_length_;
  const size_t max_query_show_length_;
  // Don't log doc suggestions for inputs shorter than |min_query_log_length_|
  // or longer than |max_query_log_length_| (i.e. don't trigger
  // field_trial_triggered and field_trial_triggered_in_session). A value of -1
  // indicates no limit. These help analyze experiments by restricting data
  // populations to avoid noise when only interested in a range of input
  // lengths. E.g. experimenting with |max_query_show_length_| would affect only
  // the small subset of long queries.
  const size_t min_query_log_length_;
  const size_t max_query_log_length_;

  // Whether a field trial has triggered for this query and this session,
  // respectively. Works similarly to BaseSearchProvider, though this class does
  // not inherit from it.
  bool field_trial_triggered_;
  bool field_trial_triggered_in_session_;

  // Whether the server has instructed us to backoff for this session (in
  // cases where the corpus is uninteresting).
  bool backoff_for_session_;

  // Client for accessing TemplateUrlService, prefs, etc.
  AutocompleteProviderClient* client_;

  // Listener to notify when results are available.
  AutocompleteProviderListener* listener_;

  // Saved when starting a new autocomplete request so that it can be retrieved
  // when responses return asynchronously.
  AutocompleteInput input_;

  // Loader used to retrieve results.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Because the drive server is async and may intermittently provide a
  // particular suggestion for consecutive inputs, without caching, doc
  // suggestions flicker between drive format (title - date - doc_type) and URL
  // format (title - URL) suggestions from the history and bookmark providers.
  // Appending cached doc suggestions with relevance 0 ensures cached
  // suggestions only display if deduped with a non-cached suggestion and do not
  // affect which autocomplete results are displayed and their ranks.
  const size_t cache_size_;
  MatchesCache matches_cache_;

  std::unique_ptr<AutocompleteProviderDebouncer> debouncer_;

  // For callbacks that may be run after destruction. Must be declared last.
  base::WeakPtrFactory<DocumentProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DocumentProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_
