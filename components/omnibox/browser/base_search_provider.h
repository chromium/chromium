// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteProviderClient;
class GURL;
class SearchTermsData;
class SuggestionDeletionHandler;
class TemplateURL;

namespace base {
class Value;
}

// Base functionality for receiving suggestions from a search engine.
// This class is abstract and should only be used as a base for other
// autocomplete providers utilizing its functionality.
class BaseSearchProvider : public AutocompleteProvider {
 public:
  BaseSearchProvider(AutocompleteProvider::Type type,
                     AutocompleteProviderClient* client);

  // Returns whether |match| is flagged as a query that should be prefetched.
  static bool ShouldPrefetch(const AutocompleteMatch& match);

  // Returns a simpler AutocompleteMatch suitable for persistence like in
  // ShortcutsDatabase.  This wrapper function uses a number of default values
  // that may or may not be appropriate for your needs.
  // NOTE: Use with care. Most likely you want the other CreateSearchSuggestion.
  static AutocompleteMatch CreateSearchSuggestion(
      const base::string16& suggestion,
      AutocompleteMatchType::Type type,
      bool from_keyword_provider,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data);

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

  // A helper function to convert result from on device providers to
  // AutocompleteMatch instance.
  static AutocompleteMatch CreateOnDeviceSearchSuggestion(
      AutocompleteProvider* autocomplete_provider,
      const AutocompleteInput& input,
      const base::string16& suggestion,
      int relevance,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      int accepted_suggestion);

  // Appends specific suggest client based on page |page_classification| to
  // the additional query params of |search_terms_args| only for Google template
  // URLs.
  static void AppendSuggestClientToAdditionalQueryParams(
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      metrics::OmniboxEventProto::PageClassification page_classification,
      TemplateURLRef::SearchTermsArgs* search_terms_args);

  // Returns whether the provided classification indicates some sort of NTP.
  static bool IsNTPPage(
      metrics::OmniboxEventProto::PageClassification classification);

  // AutocompleteProvider:
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

  bool field_trial_triggered_in_session() const {
    return field_trial_triggered_in_session_;
  }

 protected:
  // The following keys are used to record additional information on matches.

  // We annotate our AutocompleteMatches with whether their relevance scores
  // were server-provided using this key in the |additional_info| field.
  static const char kRelevanceFromServerKey[];

  // Indicates whether the server said a match should be prefetched.
  static const char kShouldPrefetchKey[];

  // Used to store metadata from the server response, which is needed for
  // prefetching.
  static const char kSuggestMetadataKey[];

  // Used to store a deletion request url for server-provided suggestions.
  static const char kDeletionUrlKey[];

  // These are the values for the above keys.
  static const char kTrue[];
  static const char kFalse[];

  ~BaseSearchProvider() override;

  typedef std::pair<base::string16, std::string> MatchKey;
  typedef std::map<MatchKey, AutocompleteMatch> MatchMap;
  typedef std::vector<std::unique_ptr<SuggestionDeletionHandler>>
      SuggestionDeletionHandlers;

  // Returns the appropriate value for the fill_into_edit field of an
  // AutcompleteMatch. The result consists of the suggestion text from
  // |suggest_result|, optionally prepended by the keyword from |template_url|
  // if |suggest_result| is from the keyword provider.
  static base::string16 GetFillIntoEdit(
      const SearchSuggestionParser::SuggestResult& suggest_result,
      const TemplateURL* template_url);

  // Callers should pass |sending_search_terms| as true if user input is being
  // sent along with the |current_page_url|.
  //
  // Returns whether we can send the URL of the current page in any suggest
  // requests.  Doing this requires that all the following hold:
  // * The suggest request is sent over HTTPS.  This avoids leaking the current
  //   page URL or personal data in unencrypted network traffic.
  // * The user has suggest enabled in their settings and is not in incognito
  //   mode.  (Incognito disables suggest entirely.)
  // * The user's suggest provider is Google.  We might want to allow other
  //   providers to see this data someday, but for now this has only been
  //   implemented for Google.
  // * The user is not on the NTP.
  // * The current URL is HTTP or HTTPS.
  // * Either one of:
  //   * The user consented to sending URLs of current page to Google and have
  //     them associated with their Google account.
  //   * The suggest endpoint and current page URL are same-origin and
  //     |sending_search_terms| is false. Same-origin suggest endpoints could
  //     have already logged the current page URL when the user accessed it, but
  //     Chrome still shouldn't leak the association between typed search terms
  //     and which tab the user is looking at. On-focus suggest requests never
  //     send search terms.
  static bool CanSendURL(
      const GURL& current_page_url,
      const GURL& suggest_url,
      const TemplateURL* template_url,
      metrics::OmniboxEventProto::PageClassification page_classification,
      const SearchTermsData& search_terms_data,
      AutocompleteProviderClient* client,
      bool sending_search_terms);

  // If the |deletion_url| is valid, then set |match.deletable| to true and
  // save the |deletion_url| into the |match|'s additional info under
  // the key |kDeletionUrlKey|.
  void SetDeletionURL(const std::string& deletion_url,
                      AutocompleteMatch* match);

  // Creates an AutocompleteMatch from |result| to search for the query in
  // |result|. Adds the created match to |map|; if such a match
  // already exists, whichever one has lower relevance is eliminated.
  // |metadata| and |accepted_suggestion| are used for generating an
  // AutocompleteMatch.
  // |mark_as_deletable| indicates whether the match should be marked deletable.
  // |in_keyword_mode| helps guarantee a non-keyword suggestion does not
  // appear as the default match when the user is in keyword mode.
  // NOTE: Any result containing a deletion URL is always marked deletable.
  void AddMatchToMap(const SearchSuggestionParser::SuggestResult& result,
                     const std::string& metadata,
                     int accepted_suggestion,
                     bool mark_as_deletable,
                     bool in_keyword_mode,
                     MatchMap* map);

  // Parses results from the suggest server and updates the appropriate suggest
  // and navigation result lists in |results|. |default_result_relevance| is
  // the relevance to use if it was not explicitly set by the server.
  // |is_keyword_result| indicates whether the response was received from the
  // keyword provider.
  // Returns whether the appropriate result list members were updated.
  bool ParseSuggestResults(const base::Value& root_val,
                           int default_result_relevance,
                           bool is_keyword_result,
                           SearchSuggestionParser::Results* results);

  // Returns the TemplateURL corresponding to the keyword or default
  // provider based on the value of |is_keyword|.
  virtual const TemplateURL* GetTemplateURL(bool is_keyword) const = 0;

  // Returns the AutocompleteInput for keyword provider or default provider
  // based on the value of |is_keyword|.
  virtual const AutocompleteInput GetInput(bool is_keyword) const = 0;

  // Returns whether the destination URL corresponding to the given |result|
  // should contain command-line-specified query params.
  virtual bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const = 0;

  // Records in UMA whether the deletion request resulted in success.
  virtual void RecordDeletionResult(bool success) = 0;

  AutocompleteProviderClient* client() { return client_; }
  const AutocompleteProviderClient* client() const { return client_; }

  bool field_trial_triggered() const { return field_trial_triggered_; }

  void set_field_trial_triggered(bool triggered) {
    field_trial_triggered_ = triggered;
  }
  void set_field_trial_triggered_in_session(bool triggered) {
    field_trial_triggered_in_session_ = triggered;
  }

 private:
  friend class SearchProviderTest;
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, TestDeleteMatch);

  // Removes the deleted |match| from the list of |matches_|.
  void DeleteMatchFromMatches(const AutocompleteMatch& match);

  // This gets called when we have requested a suggestion deletion from the
  // server to handle the results of the deletion. It will be called after the
  // deletion request completes.
  void OnDeletionComplete(bool success,
                          SuggestionDeletionHandler* handler);

  AutocompleteProviderClient* client_;

  // Whether a field trial, if any, has triggered in the most recent
  // autocomplete query. This field is set to true only if the suggestion
  // provider has completed and the response contained
  // '"google:fieldtrialtriggered":true'.
  bool field_trial_triggered_;

  // Same as above except that it is maintained across the current Omnibox
  // session.
  bool field_trial_triggered_in_session_;

  // Each deletion handler in this vector corresponds to an outstanding request
  // that a server delete a personalized suggestion. Making this a vector of
  // unique_ptr causes us to auto-cancel all such requests on shutdown.
  SuggestionDeletionHandlers deletion_handlers_;

  DISALLOW_COPY_AND_ASSIGN(BaseSearchProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_BASE_SEARCH_PROVIDER_H_
