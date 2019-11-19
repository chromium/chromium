// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the Search autocomplete provider.  This provider is
// responsible for all autocomplete entries that start with "Search <engine>
// for ...", including searching for the current input string, search
// history, and search suggestions.  An instance of it gets created and
// managed by the autocomplete controller.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCH_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCH_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/omnibox/browser/answers_cache.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

class AutocompleteProviderClient;
class AutocompleteProviderListener;
class AutocompleteResult;
class SearchProviderTest;

namespace history {
struct KeywordSearchTermVisit;
}

namespace network {
class SimpleURLLoader;
}

// Autocomplete provider for searches and suggestions from a search engine.
//
// After construction, the autocomplete controller repeatedly calls Start()
// with some user input, each time expecting to receive a small set of the best
// matches (either synchronously or asynchronously).
//
// Initially the provider creates a match that searches for the current input
// text.  It also starts a task to query the Suggest servers.  When that data
// comes back, the provider creates and returns matches for the best
// suggestions.
class SearchProvider : public BaseSearchProvider,
                       public TemplateURLServiceObserver {
 public:
  SearchProvider(AutocompleteProviderClient* client,
                 AutocompleteProviderListener* listener);

  // Extracts the suggest response metadata which SearchProvider previously
  // stored for |match|.
  static std::string GetSuggestMetadata(const AutocompleteMatch& match);

  // Answers prefetch handling - register displayed answers. Takes the top
  // match for Autocomplete and registers the contained answer data, if any.
  void RegisterDisplayedAnswers(const AutocompleteResult& result);

  // Calculates the relevance score for the keyword verbatim result (if the
  // input matches one of the profile's keywords).  If
  // |allow_exact_keyword_match| is false, the relevance for complete
  // keywords that support replacements is degraded.
  static int CalculateRelevanceForKeywordVerbatim(
      metrics::OmniboxInputType type,
      bool allow_exact_keyword_match,
      bool prefer_keyword);

  // AutocompleteProvider:
  void ResetSession() override;

  // The verbatim score for an input which is not an URL.
  static const int kNonURLVerbatimRelevance = 1300;

 protected:
  ~SearchProvider() override;

 private:
  friend class AutocompleteProviderTest;
  friend class BaseSearchProviderTest;
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, CanSendURL);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest,
                           DontInlineAutocompleteAsynchronously);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInline);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInlineDomainClassify);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationPrefixClassify);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationMidWordClassify);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationWordBreakClassify);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInlineSchemeSubstring);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, SuggestRelevanceExperiment);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, TestDeleteMatch);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, SuggestQueryUsesToken);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, SessionToken);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, AnswersCache);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, RemoveExtraAnswers);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, DoesNotProvideOnFocus);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, DoTrimHttpScheme);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest,
                           DontTrimHttpSchemeIfInputHasScheme);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest,
                           DontTrimHttpsSchemeIfInputHasScheme);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, DoTrimHttpsScheme);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderWarmUpTest, SendsWarmUpRequestOnFocus);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedPrefetchTest, ClearPrefetchedResults);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedPrefetchTest, SetPrefetchQuery);

  // Manages the providers (TemplateURLs) used by SearchProvider. Two providers
  // may be used:
  // . The default provider. This corresponds to the user's default search
  //   engine. This is always used, except for the rare case of no default
  //   engine.
  // . The keyword provider. This is used if the user has typed in a keyword.
  class Providers {
   public:
    explicit Providers(TemplateURLService* template_url_service);

    // Returns true if the specified providers match the two providers cached
    // by this class.
    bool equal(const base::string16& default_provider,
               const base::string16& keyword_provider) const {
      return (default_provider == default_provider_) &&
          (keyword_provider == keyword_provider_);
    }

    // Resets the cached providers.
    void set(const base::string16& default_provider,
             const base::string16& keyword_provider) {
      default_provider_ = default_provider;
      keyword_provider_ = keyword_provider;
    }

    const base::string16& default_provider() const { return default_provider_; }
    const base::string16& keyword_provider() const { return keyword_provider_; }

    // NOTE: These may return NULL even if the provider members are nonempty!
    const TemplateURL* GetDefaultProviderURL() const;
    const TemplateURL* GetKeywordProviderURL() const;

    // Returns true if there is a valid keyword provider.
    bool has_keyword_provider() const { return !keyword_provider_.empty(); }

   private:
    TemplateURLService* template_url_service_;

    // Cached across the life of a query so we behave consistently even if the
    // user changes their default while the query is running.
    base::string16 default_provider_;
    base::string16 keyword_provider_;

    DISALLOW_COPY_AND_ASSIGN(Providers);
  };

  class CompareScoredResults;

  typedef std::vector<history::KeywordSearchTermVisit> HistoryResults;

  // A helper function for UpdateAllOldResults().
  static void UpdateOldResults(bool minimal_changes,
                               SearchSuggestionParser::Results* results);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results,
            bool due_to_user_inactivity) override;

  // BaseSearchProvider:
  const TemplateURL* GetTemplateURL(bool is_keyword) const override;
  const AutocompleteInput GetInput(bool is_keyword) const override;
  bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const override;
  void RecordDeletionResult(bool success) override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Called back from SimpleURLLoader.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);

  // Stops the suggest query.
  // NOTE: This does not update |done_|.  Callers must do so.
  void StopSuggest();

  // Clears the current results.
  void ClearAllResults();

  // Recalculates the match contents class of |results| to better display
  // against the current input and user's language.
  void UpdateMatchContentsClass(const base::string16& input_text,
                                SearchSuggestionParser::Results* results);

  // Called after ParseSuggestResults to rank the |results|.
  void SortResults(bool is_keyword, SearchSuggestionParser::Results* results);

  // Records UMA statistics about a suggest server response.
  void LogLoadComplete(bool success, bool is_keyword);

  // Updates |matches_| from the latest results; applies calculated relevances
  // if suggested relevances cause undesirable behavior. Updates |done_|.
  void UpdateMatches();

  // Checks constraints that may be violated by suggested relevances and
  // revises/rolls back the suggested relevance scores to make all constraints
  // hold.
  void EnforceConstraints();

  // Records the top suggestion (if any) for future use.  SearchProvider tries
  // to ensure that an inline autocomplete suggestion does not change
  // asynchronously.
  void RecordTopSuggestion();

  // Called when |timer_| expires.  Sends the suggest requests.
  // If |query_is_private|, the function doesn't send this query to the default
  // provider.
  void Run(bool query_is_private);

  // Runs the history query, if necessary. The history query is synchronous.
  // This does not update |done_|.
  void DoHistoryQuery(bool minimal_changes);

  // Returns the time to delay before sending the Suggest request.
  base::TimeDelta GetSuggestQueryDelay() const;

  // Determines whether an asynchronous subcomponent query should run for the
  // current input.  If so, starts it if necessary; otherwise stops it.
  // NOTE: This function does not update |done_|.  Callers must do so.
  void StartOrStopSuggestQuery(bool minimal_changes);

  // Stops |loader| if it's running.  This includes resetting the unique_ptr.
  void CancelLoader(std::unique_ptr<network::SimpleURLLoader>* loader);

  // Returns true when the current query can be sent to at least one suggest
  // service.  This will be false for example when suggest is disabled.  In
  // the process, calculates whether the query may contain potentially
  // private data and stores the result in |is_query_private|; such queries
  // should not be sent to the default search engine.
  bool IsQuerySuitableForSuggest(bool* query_is_private) const;

  // Returns true if sending the query to a suggest server may leak sensitive
  // information (and hence the suggest request shouldn't be sent).  In
  // particular, if the input type might be a URL, we take extra care so that
  // it isn't sent to the server.
  bool IsQueryPotentiallyPrivate() const;

  // Remove existing keyword results if the user is no longer in keyword mode,
  // and, if |minimal_changes| is false, revise the existing results to
  // indicate they were received before the last keystroke.
  void UpdateAllOldResults(bool minimal_changes);

  // Given new asynchronous results, ensure that we don't clobber the current
  // top results, which were determined synchronously on the last keystroke.
  void PersistTopSuggestions(SearchSuggestionParser::Results* results);

  // Apply calculated relevance scores to the current results.
  void ApplyCalculatedSuggestRelevance(
      SearchSuggestionParser::SuggestResults* list);
  void ApplyCalculatedNavigationRelevance(
      SearchSuggestionParser::NavigationResults* list);

  // Starts a new SimpleURLLoader requesting suggest results from
  // |template_url|; callers own the returned SimpleURLLoader, which is NULL for
  // invalid providers. Note the request will never time out unless the given
  // |timeout| is greater than 0.
  std::unique_ptr<network::SimpleURLLoader> CreateSuggestLoader(
      const TemplateURL* template_url,
      const AutocompleteInput& input,
      const base::TimeDelta& timeout);

  // Converts the parsed results to a set of AutocompleteMatches, |matches_|.
  void ConvertResultsToAutocompleteMatches();

  // Remove answer contents from each match in |matches| other than the first
  // that appears.
  static void RemoveExtraAnswers(ACMatches* matches);

  // Checks if suggested relevances violate an expected constraint.
  // See UpdateMatches() for the use and explanation of this constraint
  // and other constraints enforced without the use of helper functions.
  bool IsTopMatchSearchWithURLInput() const;

  // Converts an appropriate number of navigation results in
  // |navigation_results| to matches and adds them to |matches|.
  void AddNavigationResultsToMatches(
      const SearchSuggestionParser::NavigationResults& navigation_results,
      ACMatches* matches);

  // Adds a match for each result in |raw_default_history_results_| or
  // |raw_keyword_history_results_| to |map|. |is_keyword| indicates
  // which one of the two.
  void AddRawHistoryResultsToMap(bool is_keyword,
                                 int did_not_accept_suggestion,
                                 MatchMap* map);

  // Adds a match for each transformed result in |results| to |map|.
  void AddTransformedHistoryResultsToMap(
      const SearchSuggestionParser::SuggestResults& results,
      int did_not_accept_suggestion,
      MatchMap* map);

  // Calculates relevance scores for all |results|.
  SearchSuggestionParser::SuggestResults ScoreHistoryResultsHelper(
      const HistoryResults& results,
      bool base_prevent_inline_autocomplete,
      bool input_multiple_words,
      const base::string16& input_text,
      bool is_keyword);

  // Calculates relevance scores for |results|, adjusting for boundary
  // conditions around multi-word queries. (See inline comments in function
  // definition for more details.)
  void ScoreHistoryResults(
      const HistoryResults& results,
      bool is_keyword,
      SearchSuggestionParser::SuggestResults* scored_results);

  // Adds matches for |results| to |map|.
  void AddSuggestResultsToMap(
      const SearchSuggestionParser::SuggestResults& results,
      const std::string& metadata,
      MatchMap* map);

  // Gets the relevance score for the verbatim result.  This value may be
  // provided by the suggest server or calculated locally; if
  // |relevance_from_server| is non-null, it will be set to indicate which of
  // those is true.
  int GetVerbatimRelevance(bool* relevance_from_server) const;

  // Whether we should limit suggestions from SearchProvider while in
  // keyword mode to only keyword suggestions. Used when we suspect that the
  // user intentionally entered keyword mode and doesn't want the others.
  bool ShouldCurbDefaultSuggestions() const;

  // Calculates the relevance score for the verbatim result from the
  // default search engine.  This version takes into account context:
  // i.e., whether the user has entered a keyword-based search or not.
  int CalculateRelevanceForVerbatim() const;

  // Calculates the relevance score for the verbatim result from the default
  // search engine *ignoring* whether the input is a keyword-based search
  // or not.  This function should only be used to determine the minimum
  // relevance score that the best result from this provider should have.
  // For normal use, prefer the above function.
  int CalculateRelevanceForVerbatimIgnoringKeywordModeState() const;

  // Gets the relevance score for the keyword verbatim result.
  // |relevance_from_server| is handled as in GetVerbatimRelevance().
  // TODO(mpearson): Refactor so this duplication isn't necessary or
  // restructure so one static function takes all the parameters it needs
  // (rather than looking at internal state).
  int GetKeywordVerbatimRelevance(bool* relevance_from_server) const;

  // |time| is the time at which this query was last seen.  |is_keyword|
  // indicates whether the results correspond to the keyword provider or default
  // provider. |use_aggressive_method| says whether this function can use a
  // method that gives high scores (1200+) rather than one that gives lower
  // scores.  When using the aggressive method, scores may exceed 1300
  // unless |prevent_search_history_inlining| is set.
  int CalculateRelevanceForHistory(const base::Time& time,
                                   bool is_keyword,
                                   bool use_aggressive_method,
                                   bool prevent_search_history_inlining) const;

  // Returns an AutocompleteMatch for a navigational suggestion.
  AutocompleteMatch NavigationToMatch(
      const SearchSuggestionParser::NavigationResult& navigation);

  // Updates the value of |done_| from the internal state.
  void UpdateDone();

  // Obtains a session token, regenerating if necessary.
  std::string GetSessionToken();

  // Answers prefetch handling - finds the previously displayed answer matching
  // the current top-scoring history result. If there is a previous answer,
  // returns the query data associated with it. Otherwise, returns an empty
  // AnswersQueryData.
  AnswersQueryData FindAnswersPrefetchData();

  // Finds image URLs in most relevant results and uses client to prefetch them.
  void PrefetchImages(SearchSuggestionParser::Results* results);

  AutocompleteProviderListener* listener_;

  // Maintains the TemplateURLs used.
  Providers providers_;

  // The user's input.
  AutocompleteInput input_;

  // Input when searching against the keyword provider.
  AutocompleteInput keyword_input_;

  // Searches in the user's history that begin with the input text.
  HistoryResults raw_keyword_history_results_;
  HistoryResults raw_default_history_results_;

  // Scored searches in the user's history - based on |keyword_history_results_|
  // or |default_history_results_| as appropriate.
  SearchSuggestionParser::SuggestResults transformed_keyword_history_results_;
  SearchSuggestionParser::SuggestResults transformed_default_history_results_;

  // A timer to start a query to the suggest server after the user has stopped
  // typing for long enough.
  base::OneShotTimer timer_;

  // The time at which we sent a query to the suggest server.
  base::TimeTicks time_suggest_request_sent_;

  // Loaders used to retrieve results for the keyword and default providers.
  // After a loader's results are returned, it gets reset, so a non-null
  // loader indicates that loader is still in flight.
  std::unique_ptr<network::SimpleURLLoader> keyword_loader_;
  std::unique_ptr<network::SimpleURLLoader> default_loader_;

  // Results from the default and keyword search providers.
  SearchSuggestionParser::Results default_results_;
  SearchSuggestionParser::Results keyword_results_;

  // The top query suggestion, left blank if none.
  base::string16 top_query_suggestion_fill_into_edit_;
  // The top navigation suggestion, left blank/invalid if none.
  GURL top_navigation_suggestion_;

  // Session token management.
  std::string current_token_;
  base::TimeTicks token_expiration_time_;

  // Answers prefetch management.
  AnswersCache answers_cache_;  // Cache for last answers seen.
  AnswersQueryData prefetch_data_;  // Data to use for query prefetching.

  ScopedObserver<TemplateURLService, TemplateURLServiceObserver> observer_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SearchProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCH_PROVIDER_H_
