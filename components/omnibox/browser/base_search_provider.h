// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class contains common functionality for search-based autocomplete
// providers. Search provider and zero suggest provider both use it for common
// functionality.

#ifndef COMPONENTS_OMNIBOX_BROWSER_BASE_SEARCH_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_BASE_SEARCH_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"

class AutocompleteProviderClient;
class GURL;
class SearchTermsData;
class TemplateURL;

namespace network {
class SimpleURLLoader;
}

// Base functionality for receiving suggestions from a search engine.
// This class is abstract and should only be used as a base for other
// autocomplete providers utilizing its functionality.
class BaseSearchProvider : public AutocompleteProvider {
 public:
  using ScoringSignals = ::metrics::OmniboxScoringSignals;

  BaseSearchProvider(AutocompleteProvider::Type type,
                     AutocompleteProviderClient* client);

  BaseSearchProvider(const BaseSearchProvider&) = delete;
  BaseSearchProvider& operator=(const BaseSearchProvider&) = delete;

  // Returns whether |match| is flagged as a query that should be prefetched.
  static bool ShouldPrefetch(const AutocompleteMatch& match);

  // Returns whether |match| is flagged as a query that should be prerendered.
  static bool ShouldPrerender(const AutocompleteMatch& match);

  // Returns an AutocompleteMatch with the given |autocomplete_provider|
  // for the search |suggestion|, which represents a search via |template_url|.
  // If |template_url| is NULL, returns a match with an invalid destination URL.
  //
  // |input| is the original user input. Text in the input is used to highlight
  // portions of the match contents to distinguish locally-typed text from
  // suggested text.
  //
  // |input| is also necessary for various other details, like whether we should
  // allow inline autocompletion and what the transition type should be.
  // |in_keyword_mode| helps guarantee a non-keyword suggestion does not
  // appear as the default match when the user is in keyword mode.
  // |accepted_suggestion| is used to generate Assisted Query Stats.
  // |append_extra_query_params_from_command_line| should be set if
  // |template_url| is the default search engine, so the destination URL will
  // contain any command-line-specified query params.
  static AutocompleteMatch CreateSearchSuggestion(
      AutocompleteProvider* autocomplete_provider,
      const AutocompleteInput& input,
      const bool in_keyword_mode,
      const SearchSuggestionParser::SuggestResult& suggestion,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      int accepted_suggestion,
      bool append_extra_query_params_from_command_line);

  // A helper function to return an AutocompleteMatch suitable for persistence
  // in ShortcutsDatabase.
  static AutocompleteMatch CreateShortcutSearchSuggestion(
      const std::u16string& suggestion,
      AutocompleteMatchType::Type type,
      bool from_keyword_provider,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data);

  // A helper function to return an AutocompleteMatch for on device provider.
  static AutocompleteMatch CreateOnDeviceSearchSuggestion(
      AutocompleteProvider* autocomplete_provider,
      const AutocompleteInput& input,
      const std::u16string& suggestion,
      int relevance,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      int accepted_suggestion,
      bool is_tail_suggestion);

  static scoped_refptr<OmniboxAction> CreateActionInSuggest(
      omnibox::ActionInfo action_info,
      const TemplateURLRef& search_url,
      const TemplateURLRef::SearchTermsArgs& original_search_terms,
      const SearchTermsData& search_terms_data);

  static scoped_refptr<OmniboxAction> CreateAnswerAction(
      omnibox::SuggestionEnhancement enhancement,
      TemplateURLRef::SearchTermsArgs search_terms_args,
      omnibox::AnswerType answer_type);

  // Returns whether the URL of the current page is eligible to be sent in any
  // suggest request. Only valid URLs with an HTTP or HTTPS scheme are eligible.
  static bool PageURLIsEligibleForSuggestRequest(const GURL& page_url);
  // Returns whether a suggest request can be made without the current page URL.
  // It requires that all the following to hold:
  // * The suggest request is sent over HTTPS. This avoids leaking the current
  //   page URL or personal data in unencrypted network traffic.
  // * The user has suggest enabled in their settings.
  // * The user is not in incognito mode. Incognito disables suggest entirely.
  // * The user's suggest provider is Google. We might want to allow other
  //   providers to see this data someday, but for now this has only been
  //   implemented for Google.
  static bool CanSendSuggestRequestWithoutPageURL(
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      const AutocompleteProviderClient* client);
  // Returns whether a suggest request can be made with the current page URL.
  // It requires that all the following hold:
  // * CanSendSuggestRequestWithoutPageURL() returns true.
  // * Either one of:
  //   * The user consented to sending URLs of current page to Google and have
  //     them associated with their Google account.
  //   * The current page URL is the Search Results Page. The suggest endpoint
  //     could have logged the page URL when the user accessed it.
  static bool CanSendSuggestRequestWithPageURL(
      const GURL& current_page_url,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      const AutocompleteProviderClient* client);

  // AutocompleteProvider:
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

 protected:
  // The following keys are used to record additional information on matches.

  // We annotate our AutocompleteMatches with whether their relevance scores
  // were server-provided using this key in the |additional_info| field.
  static const char kRelevanceFromServerKey[];

  // Indicates whether the server said a match should be prefetched.
  static const char kShouldPrefetchKey[];

  // Indicates whether the server said a match should be prerendered by
  // Prerender2. See content/browser/preloading/prerender/README.md for more
  // information.
  static const char kShouldPrerenderKey[];

  // Used to store a deletion request url for server-provided suggestions.
  static const char kDeletionUrlKey[];

  // These are the values for the above keys.
  static const char kTrue[];
  static const char kFalse[];

  ~BaseSearchProvider() override;

  using MatchKey = ACMatchKey<std::u16string, std::string>;
  using MatchMap = std::map<MatchKey, AutocompleteMatch>;

  // Returns the appropriate value for the fill_into_edit field of an
  // AutcompleteMatch. The result consists of the suggestion text from
  // |suggest_result|, optionally prepended by the keyword from |template_url|
  // if |suggest_result| is from the keyword provider.
  static std::u16string GetFillIntoEdit(
      const SearchSuggestionParser::SuggestResult& suggest_result,
      const TemplateURL* template_url);

  // If the |deletion_url| is valid, then set |match.deletable| to true and
  // save the |deletion_url| into the |match|'s additional info under
  // the key |kDeletionUrlKey|.
  void SetDeletionURL(const std::string& deletion_url,
                      AutocompleteMatch* match);

  // Creates an `AutocompleteMatch` from `result` and `input` to search for the
  // query in `result`. Adds the created match to `map`; if such a match already
  // exists, whichever one has lower relevance is eliminated.
  // `accepted_suggestion` is used for generating an `AutocompleteMatch`.
  // `mark_as_deletable` indicates whether the match should be marked deletable.
  // `in_keyword_mode` helps guarantee a non-keyword suggestion does not appear
  // as the default match when the user is in keyword mode. NOTE: Any result
  // containing a deletion URL is always marked deletable.
  void AddMatchToMap(const SearchSuggestionParser::SuggestResult& result,
                     const AutocompleteInput& input,
                     const TemplateURL* template_url,
                     const SearchTermsData& search_terms_data,
                     int accepted_suggestion,
                     bool mark_as_deletable,
                     bool in_keyword_mode,
                     MatchMap* map);

  // Returns whether the destination URL corresponding to the given `result`
  // should contain command-line-specified query params.
  virtual bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const = 0;

  // Records in UMA whether the deletion request resulted in
  // success.
  virtual void RecordDeletionResult(bool success) = 0;

  AutocompleteProviderClient* client() { return client_; }
  const AutocompleteProviderClient* client() const { return client_; }

 private:
  friend class SearchProviderTest;
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, TestDeleteMatch);

  // Removes the deleted |match| from the list of |matches_|.
  void DeleteMatchFromMatches(const AutocompleteMatch& match);

  // This gets called when we have requested a suggestion
  // deletion from the server to handle the results of the
  // deletion. It will be called after the deletion request
  // completes.
  void OnDeletionComplete(const network::SimpleURLLoader* source,
                          const int response_code,
                          std::unique_ptr<std::string> response_body);

  raw_ptr<AutocompleteProviderClient> client_;

  // Each deletion loader in this vector corresponds to an
  // outstanding request that a server delete a personalized
  // suggestion. Making this a vector of unique_ptr causes us
  // to auto-cancel all such requests on shutdown.
  std::vector<std::unique_ptr<network::SimpleURLLoader>> deletion_loaders_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_BASE_SEARCH_PROVIDER_H_
