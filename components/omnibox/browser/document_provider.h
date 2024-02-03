// Copyright 2018 The Chromium Authors
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
#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
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
                                  AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

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
  static ACMatchClassifications Classify(const std::u16string& input_text,
                                         const std::u16string& text);

  // Builds a GURL to use for deduping against other document/history
  // suggestions. Multiple URLs may refer to the same document.
  // Returns an empty GURL if not a recognized Docs URL.
  // The URL returned is not guaranteed to be navigable and should only be used
  // as a deduping token.
  static const GURL GetURLForDeduping(const GURL& url);

 private:
  friend class FakeDocumentProvider;

  using MatchesCache = base::LRUCache<GURL, AutocompleteMatch>;

  DocumentProvider(AutocompleteProviderClient* client,
                   AutocompleteProviderListener* listener);

  ~DocumentProvider() override;

  DocumentProvider(const DocumentProvider&) = delete;
  DocumentProvider& operator=(const DocumentProvider&) = delete;

  // Determines whether the profile/session/window meet the feature
  // prerequisites.
  bool IsDocumentProviderAllowed(const AutocompleteInput& input);

  // Determines if the input is a URL, or is the start of the user entering one.
  // We avoid queries for these cases for quality and scaling reasons.
  static bool IsInputLikelyURL(const AutocompleteInput& input);

  // Called by |debouncer_|, queued when |start| is called.
  void Run();

  // Called when the network request for suggestions has completed.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         const int response_code,
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
  // according to |input_.text()|.
  void CopyCachedMatchesToMatches();

  // Sets the scores of all cached matches to 0. This is invoked before pushing
  // the latest async response returns so that the scores aren't preserved for
  // further inputs. E.g., the input 'london' shouldn't display cached docs from
  // a previous input 'paris'. This can't be done by automatically (i.e. set
  // scores to 0 before pushing to the cache), as the scores are needed for the
  // async pass if the user continued their input.
  void SetCachedMatchesScoresTo0();

  // Sets the scores of matches beyond the first |provider_max_matches_| to 0.
  // This ensures the doc provider doesn't exceed it's allocated suggestions
  // while also allowing docs from other providers to be deduped and styled like
  // docs from the doc provider.
  void DemoteMatchesBeyondMax();

  // Generates the localized last-modified timestamp to present to the user.
  // Full date for old files, mm/dd within the same calendar year, or time-of-
  // day if a file was modified on the same date.
  // |now| should generally be base::Time::Now() but is passed in for testing.
  static std::u16string GenerateLastModifiedString(
      const std::string& modified_timestamp_string,
      base::Time now);

  // Convert mimetype (e.g. "application/vnd.google-apps.document") to a string
  // that can be used in the match description (e.g. "Google Docs").
  static std::u16string GetProductDescriptionString(
      const std::string& mimetype);

  // Construct match description; e.g. "Jan 12 - First Last - Google Docs".
  static std::u16string GetMatchDescription(const std::string& update_time,
                                            const std::string& mimetype,
                                            const std::string& owner);

  // Whether the server has instructed us to backoff for this session (in
  // cases where the corpus is uninteresting).
  bool backoff_for_session_;

  // Client for accessing TemplateUrlService, prefs, etc.
  raw_ptr<AutocompleteProviderClient> client_;

  // Saved when starting a new autocomplete request so that it can be retrieved
  // when responses return asynchronously.
  AutocompleteInput input_;

  // Loader used to retrieve results.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // The time `Run()` was invoked. Used for histogram logging.
  base::TimeTicks time_run_invoked_;
  // The time `OnDocumentSuggestionsLoaderAvailable()` was invoked and the
  // remote request was sent. Used for histogram logging.
  base::TimeTicks time_request_sent_;

  // Because the drive server is async and may intermittently provide a
  // particular suggestion for consecutive inputs, without caching, doc
  // suggestions flicker between drive format (title - date - doc_type) and URL
  // format (title - URL) suggestions from the history and bookmark providers.
  // Appending cached doc suggestions with relevance 0 ensures cached
  // suggestions only display if deduped with a non-cached suggestion and do not
  // affect which autocomplete results are displayed and their ranks.
  MatchesCache matches_cache_;

  std::unique_ptr<AutocompleteProviderDebouncer> debouncer_;

  // For callbacks that may be run after destruction. Must be declared last.
  base::WeakPtrFactory<DocumentProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_
