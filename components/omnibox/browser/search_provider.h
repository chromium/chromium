// Copyright 2012 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
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
  SearchProvider(const SearchProvider&) = delete;
  SearchProvider& operator=(const SearchProvider&) = delete;

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

  // The verbatim score for an input which is not a URL.
  static const int kNonURLVerbatimRelevance = 1300;

  // Returns whether the current page URL can be sent in the suggest requests.
  // This method is virtual to mock for testing.
  virtual bool CanSendCurrentPageURLInRequest(
      const GURL& current_page_url,
      metrics::OmniboxEventProto::PageClassification page_classification,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      const AutocompleteProviderClient* client);

 protected:
  ~SearchProvider() override;

 private:
  friend class AutocompleteProviderTest;
  friend class BaseSearchProviderTest;
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
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, AnswersCache);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, RemoveExtraAnswers);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, DuplicateCardAnswer);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, CopyAnswerToVerbatim);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, DoesNotProvideOnFocus);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, SendsWarmUpRequestOnFocus);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, DoTrimHttpScheme);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest,
                           DontTrimHttpSchemeIfInputHasScheme);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest,
                           DontTrimHttpsSchemeIfInputHasScheme);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, DoTrimHttpsScheme);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderRequestTest, SendRequestWithURL);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderRequestTest, SendRequestWithoutURL);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderRequestTest,
                           SendRequestWithLensInteractionResponse);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderRequestTest,
                           SendRequestWithoutLensInteractionResponse);

  // Manages the providers (TemplateURLs) used by SearchProvider. Two providers
  // may be used:
  // . The default provider. This corresponds to the user's default search
  //   engine. This is always used, except for the rare case of no default
  //   engine.
  // . The keyword provider. This is used if the user has typed in a keyword.
  class Providers {
   public:
    explicit Providers(TemplateURLService* template_url_service);
    Providers(const Providers&) = delete;
    Providers& operator=(const Providers&) = delete;

    // Returns true if the specified providers match the two providers cached
    // by this class.
    bool equal(const std::u16string& default_provider,
               const std::u16string& keyword_provider) const {
      return (default_provider == default_provider_) &&
          (keyword_provider == keyword_provider_);
    }

    // Resets the cached providers.
    void set(const std::u16string& default_provider,
             const std::u16string& keyword_provider) {
      default_provider_ = default_provider;
      keyword_provider_ = keyword_provider;
    }

    const std::u16string& default_provider() const { return default_provider_; }
    const std::u16string& keyword_provider() const { return keyword_provider_; }

    // NOTE: These may return NULL even if the provider members are nonempty!
    const TemplateURL* GetDefaultProviderURL() const;
    const TemplateURL* GetKeywordProviderURL() const;

    // Returns true if there is a valid keyword provider.
    bool has_keyword_provider() const { return !keyword_provider_.empty(); }

   private:
    raw_ptr<TemplateURLService> template_url_service_;

    // Cached across the life of a query so we behave consistently even if the
    // user changes their default while the query is running.
    std::u16string default_provider_;
    std::u16string keyword_provider_;
  };

  class CompareScoredResults;

  typedef std::vector<std::unique_ptr<history::KeywordSearchTermVisit>>
      HistoryResults;

  // A helper function for UpdateAllOldResults().
  static void UpdateOldResults(bool minimal_changes,
                               SearchSuggestionParser::Results* results);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results,
            bool due_to_user_inactivity) override;

  // BaseSearchProvider:
  bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const override;
  void RecordDeletionResult(bool success) override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Returns the TemplateURL corresponding to the keyword or default
  // provider based on the value of |is_keyword|.
  const TemplateURL* GetTemplateURL(bool is_keyword) const;

  // Returns the AutocompleteInput for keyword provider or default provider
  // based on the value of |is_keyword|.
  const AutocompleteInput GetInput(bool is_keyword) const;

  // Called back from SimpleURLLoader.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         const int response_code,
                         std::unique_ptr<std::string> response_body);

  // Stops the suggest query.
  // NOTE: This does not update |done_|.  Callers must do so.
  void StopSuggest();

  // Clears the current results.
  void ClearAllResults();

  // Recalculates the match contents class of |results| to better display
  // against the current input and user's language.
  void UpdateMatchContentsClass(const std::u16string& input_text,
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
  // invalid providers.
  std::unique_ptr<network::SimpleURLLoader> CreateSuggestLoader(
      const TemplateURL* template_url,
      const AutocompleteInput& input);

  // Converts the parsed results to a set of AutocompleteMatches, |matches_|.
  void ConvertResultsToAutocompleteMatches();

  // Remove answer contents from each match in |matches| other than the first
  // that appears.
  static void RemoveExtraAnswers(ACMatches* matches);

  // Add a copy of an answer suggestion presented as a rich card, sans answer
  // data. This gives an "escape hatch" if, e.g. the user wants the verbatim
  // query associated with the answer suggestion.
  static void DuplicateCardAnswer(ACMatches* matches);

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
      const std::u16string& input_text,
      bool is_keyword);

  // Calculates relevance scores for |results|, adjusting for boundary
  // conditions around multi-word queries. (See inline comments in function
  // definition for more details.)
  void ScoreHistoryResults(
      const HistoryResults& results,
      bool is_keyword,
      SearchSuggestionParser::SuggestResults* scored_results);

  // Adds matches for `results` to `map`.
  void AddSuggestResultsToMap(
      const SearchSuggestionParser::SuggestResults& results,
      MatchMap* map);

  // Gets the relevance score for the verbatim result.  This value may be
  // provided by the suggest server or calculated locally; if
  // |relevance_from_server| is non-null, it will be set to indicate which of
  // those is true.
  int GetVerbatimRelevance(bool* relevance_from_server) const;

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
  // scores.  When using the aggressive method, scores may exceed 1300.
  int CalculateRelevanceForHistory(const base::Time& time,
                                   bool is_keyword,
                                   bool use_aggressive_method) const;

  // Returns an AutocompleteMatch for a navigational suggestion.
  AutocompleteMatch NavigationToMatch(
      const SearchSuggestionParser::NavigationResult& navigation);

  // Updates the value of |done_| from the internal state.
  void UpdateDone();

  // Answers prefetch handling - finds the previously displayed answer matching
  // the current top-scoring history result. If there is a previous answer,
  // returns the query data associated with it. Otherwise, returns an empty
  // AnswersQueryData.
  AnswersQueryData FindAnswersPrefetchData();

  // Finds image URLs in most relevant results and uses client to prefetch them.
  void PrefetchImages(SearchSuggestionParser::Results* results);

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
  std::u16string top_query_suggestion_fill_into_edit_;
  // The top navigation suggestion, left blank/invalid if none.
  GURL top_navigation_suggestion_;

  // Answers prefetch management.
  AnswersCache answers_cache_;  // Cache for last answers seen.
  AnswersQueryData prefetch_data_;  // Data to use for query prefetching.

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      observation_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCH_PROVIDER_H_
