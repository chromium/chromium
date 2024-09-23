// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCH_SUGGESTION_PARSER_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCH_SUGGESTION_PARSER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/values.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/chrome_searchbox_stats.pb.h"
#include "third_party/omnibox_proto/entity_info.pb.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "url/gurl.h"

class AutocompleteInput;
class AutocompleteSchemeClassifier;

namespace network {
class SimpleURLLoader;
}

// Returns the omnibox::SuggestSubtype enum object corresponding to `value`.
omnibox::SuggestSubtype SuggestSubtypeForNumber(int value);

class SearchSuggestionParser {
 public:
  // Disallow implicit constructors.
  SearchSuggestionParser() = delete;
  SearchSuggestionParser(const SearchSuggestionParser&) = delete;
  SearchSuggestionParser& operator=(const SearchSuggestionParser&) = delete;

  // The Result classes are intermediate representations of AutocompleteMatches,
  // simply containing relevance-ranked search and navigation suggestions.
  // They may be cached to provide some synchronous matches while requests for
  // new suggestions from updated input are in flight.
  // TODO(msw) Extend these classes to generate their corresponding matches and
  //           other requisite data, in order to consolidate and simplify the
  //           highly fragmented SearchProvider logic for each Result type.
  class Result {
   public:
    Result(bool from_keyword,
           int relevance,
           bool relevance_from_server,
           AutocompleteMatchType::Type type,
           omnibox::SuggestType suggest_type,
           std::vector<int> subtypes,
           const std::string& deletion_url,
           omnibox::NavigationalIntent navigational_intent);
    Result(const Result& other);
    virtual ~Result();

    bool from_keyword() const { return from_keyword_; }

    const std::u16string& match_contents() const { return match_contents_; }
    const ACMatchClassifications& match_contents_class() const {
      return match_contents_class_;
    }

    AutocompleteMatchType::Type type() const { return type_; }
    omnibox::SuggestType suggest_type() const { return suggest_type_; }
    const std::vector<int>& subtypes() const { return subtypes_; }
    int relevance() const { return relevance_; }
    void set_relevance(int relevance) { relevance_ = relevance; }
    bool received_after_last_keystroke() const {
      return received_after_last_keystroke_;
    }
    void set_received_after_last_keystroke(bool received_after_last_keystroke) {
      received_after_last_keystroke_ = received_after_last_keystroke;
    }

    bool relevance_from_server() const { return relevance_from_server_; }
    void set_relevance_from_server(bool relevance_from_server) {
      relevance_from_server_ = relevance_from_server;
    }

    const std::string& deletion_url() const { return deletion_url_; }

    omnibox::NavigationalIntent navigational_intent() const {
      return navigational_intent_;
    }

    // Returns the default relevance value for this result (which may
    // be left over from a previous omnibox input) given the current
    // input and whether the current input caused a keyword provider
    // to be active.
    virtual int CalculateRelevance(const AutocompleteInput& input,
                                   bool keyword_provider_requested) const = 0;

   protected:
    // The contents to be displayed and its style info.
    std::u16string match_contents_;
    ACMatchClassifications match_contents_class_;

    // True if the result came from a keyword suggestion.
    bool from_keyword_;

    // AutocompleteMatch type.
    AutocompleteMatchType::Type type_;

    // Suggestion type.
    omnibox::SuggestType suggest_type_;

    // Suggestion subtypes.
    std::vector<int> subtypes_;

    // The relevance score.
    int relevance_;

   private:
    // Whether this result's relevance score was fully or partly calculated
    // based on server information, and thus is assumed to be more accurate.
    // This is ultimately used in
    // SearchProvider::ConvertResultsToAutocompleteMatches(), see comments
    // there.
    bool relevance_from_server_;

    // Whether this result was received asynchronously after the last
    // keystroke, otherwise it must have come from prior cached results
    // or from a synchronous provider.
    bool received_after_last_keystroke_;

    // Optional deletion URL provided with suggestions. Fetching this URL
    // should result in some reasonable deletion behaviour on the server,
    // e.g. deleting this term out of a user's server-side search history.
    std::string deletion_url_;

    // The "navigational intent" of this result. In other words, the likelihood
    // that the user intends to navigate to a specific place by making use of
    // this result.
    omnibox::NavigationalIntent navigational_intent_ = omnibox::NAV_INTENT_NONE;
  };

  class SuggestResult : public Result {
   public:
    SuggestResult(const std::u16string& suggestion,
                  AutocompleteMatchType::Type type,
                  omnibox::SuggestType suggest_type,
                  std::vector<int> subtypes,
                  bool from_keyword,
                  omnibox::NavigationalIntent navigational_intent,
                  int relevance,
                  bool relevance_from_server,
                  const std::u16string& input_text);
    SuggestResult(const std::u16string& suggestion,
                  AutocompleteMatchType::Type type,
                  omnibox::SuggestType suggest_type,
                  std::vector<int> subtypes,
                  const std::u16string& match_contents,
                  const std::u16string& match_contents_prefix,
                  const std::u16string& annotation,
                  omnibox::EntityInfo entity_info,
                  const std::string& deletion_url,
                  bool from_keyword,
                  omnibox::NavigationalIntent navigational_intent,
                  int relevance,
                  bool relevance_from_server,
                  bool should_prefetch,
                  bool should_prerender,
                  const std::u16string& input_text);
    SuggestResult(const SuggestResult& result);
    ~SuggestResult() override;

    SuggestResult& operator=(const SuggestResult& rhs);

    const std::u16string& suggestion() const { return suggestion_; }
    const std::u16string& match_contents_prefix() const {
      return match_contents_prefix_;
    }
    const std::u16string& annotation() const { return annotation_; }

    void set_suggestion_group_id(
        std::optional<omnibox::GroupId> suggestion_group_id) {
      suggestion_group_id_ = suggestion_group_id;
    }
    std::optional<omnibox::GroupId> suggestion_group_id() const {
      return suggestion_group_id_;
    }

    void SetAnswer(const SuggestionAnswer& answer);
    const std::optional<SuggestionAnswer>& answer() const { return answer_; }

    void SetRichAnswerTemplate(
        const omnibox::RichAnswerTemplate& answer_template);
    const std::optional<omnibox::RichAnswerTemplate>& answer_template() const {
      return answer_template_;
    }

    void SetAnswerType(const omnibox::AnswerType& answer_type);
    const omnibox::AnswerType& answer_type() const { return answer_type_; }

    void SetEntityInfo(const omnibox::EntityInfo&);
    const omnibox::EntityInfo& entity_info() const { return entity_info_; }

    bool should_prefetch() const { return should_prefetch_; }
    bool should_prerender() const { return should_prerender_; }

    // Fills in |match_contents_class_| to reflect how |match_contents_| should
    // be displayed and bolded against the current |input_text|.  If
    // |allow_bolding_all| is false and |match_contents_class_| would have all
    // of |match_contents_| bolded, do nothing.
    void ClassifyMatchContents(const bool allow_bolding_all,
                               const std::u16string& input_text);

    // Result:
    int CalculateRelevance(const AutocompleteInput& input,
                           bool keyword_provider_requested) const override;

   private:
    // The search terms to be used for this suggestion.
    std::u16string suggestion_;

    // The contents to be displayed as prefix of match contents.
    // Used for tail suggestions to display a leading ellipsis (or some
    // equivalent character) to indicate omitted text.
    // Only used to pass this information to about:omnibox's "Additional Info".
    std::u16string match_contents_prefix_;

    // Optional annotation for the |match_contents_| for disambiguation.
    // This may be displayed in the autocomplete match contents, but is defined
    // separately to facilitate different formatting.
    std::u16string annotation_;

    // The optional suggestion group ID used to look up the suggestion group
    // config for the group this suggestion belongs to from the server response.
    std::optional<omnibox::GroupId> suggestion_group_id_;

    // Optional short answer to the input that produced this suggestion.
    std::optional<SuggestionAnswer> answer_;

    // Optional proto that contains answer info for rich answers.
    std::optional<omnibox::RichAnswerTemplate> answer_template_;

    // Answer type for answer verticals, including rich answers.
    omnibox::AnswerType answer_type_ = omnibox::ANSWER_TYPE_UNSPECIFIED;

    // Proto containing various pieces of data related to entity suggestions.
    omnibox::EntityInfo entity_info_;

    // Should this result be prefetched?
    bool should_prefetch_;

    // Should this result trigger Prerender2? See
    // content/browser/preloading/prerender/README.md for more information.
    bool should_prerender_;
  };

  class NavigationResult : public Result {
   public:
    NavigationResult(const AutocompleteSchemeClassifier& scheme_classifier,
                     const GURL& url,
                     AutocompleteMatchType::Type type,
                     omnibox::SuggestType suggest_type,
                     std::vector<int> subtypes,
                     const std::u16string& description,
                     const std::string& deletion_url,
                     bool from_keyword,
                     omnibox::NavigationalIntent navigational_intent,
                     int relevance,
                     bool relevance_from_server,
                     const std::u16string& input_text);
    NavigationResult(const NavigationResult& other);
    ~NavigationResult() override;

    const GURL& url() const { return url_; }
    const std::u16string& description() const { return description_; }
    const ACMatchClassifications& description_class() const {
      return description_class_;
    }
    const std::u16string& formatted_url() const { return formatted_url_; }

    // Fills in |match_contents_| and |match_contents_class_| to reflect how
    // the URL should be displayed and bolded against the current |input_text|.
    // If |allow_bolding_nothing| is false and |match_contents_class_| would
    // result in an entirely unbolded |match_contents_|, do nothing.
    void CalculateAndClassifyMatchContents(const bool allow_bolding_nothing,
                                           const std::u16string& input_text);

    // Result:
    int CalculateRelevance(const AutocompleteInput& input,
                           bool keyword_provider_requested) const override;

   private:
    void ClassifyDescription(const std::u16string& input_text);

    // The suggested url for navigation.
    GURL url_;

    // The properly formatted ("fixed up") URL string with equivalent meaning
    // to the one in |url_|.
    std::u16string formatted_url_;

    // The suggested navigational result description; generally the site name.
    std::u16string description_;
    ACMatchClassifications description_class_;
  };

  typedef std::vector<SuggestResult> SuggestResults;
  typedef std::vector<NavigationResult> NavigationResults;
  typedef std::vector<omnibox::metrics::ChromeSearchboxStats::ExperimentStatsV2>
      ExperimentStatsV2s;

  // A simple structure bundling most of the information (including
  // both SuggestResults and NavigationResults) returned by a call to
  // the suggest server.
  //
  // This has to be declared after the typedefs since it relies on some of them.
  struct Results {
    Results();
    ~Results();
    Results(const Results&) = delete;
    Results& operator=(const Results&) = delete;

    // Clears |suggest_results| and |navigation_results| and resets
    // |verbatim_relevance| to -1 (implies unset).
    void Clear();

    // Returns whether any of the results (including verbatim) have
    // server-provided scores.
    bool HasServerProvidedScores() const;

    // Query suggestions sorted by relevance score, descending. This order is
    // normally provided by server and is guaranteed after search provider
    // calls SortResults, so order always holds except possibly while parsing.
    SuggestResults suggest_results;

    // Navigational suggestions sorted by relevance score, descending. This
    // order is normally provided by server and is guaranteed after search
    // provider calls SortResults, so order always holds except possibly while
    // parsing.
    NavigationResults navigation_results;

    // The server supplied verbatim relevance scores. Negative values
    // indicate that there is no suggested score; a value of 0
    // suppresses the verbatim result.
    int verbatim_relevance;

    // The JSON metadata associated with this server response.
    std::string metadata;

    // If the active suggest field trial (if any) has triggered.
    bool field_trial_triggered;

    // The ExperimentStatsV2 containing GWS experiment details, if any. To be
    // logged to SearchboxStats.
    ExperimentStatsV2s experiment_stats_v2s;

    // If the relevance values of the results are from the server.
    bool relevances_from_server;

    // The map of suggestion group IDs to suggestion group information.
    omnibox::GroupConfigMap suggestion_groups_map;
  };

  // Converts JSON loaded by a SimpleURLLoader into UTF-8 and returns the
  // result.
  //
  // |source| must be the SimpleURLLoader that loaded the data; it is used to
  // lookup the body's encoding from response headers.
  // Note: It can be nullptr in tests.
  //
  // |response_body| must be the body of the response; it may be null.
  static std::string ExtractJsonData(
      const network::SimpleURLLoader* source,
      std::unique_ptr<std::string> response_body);

  // Parses JSON response received from the provider, stripping XSSI
  // protection if needed. Returns the parsed data if successful, NULL
  // otherwise.
  static std::optional<base::Value::List> DeserializeJsonData(
      std::string_view json_data);

  // Parses results from the suggest server and updates the appropriate suggest
  // and navigation result lists in |results|. |is_keyword_result| indicates
  // whether the response was received from the keyword provider.
  // Returns whether the appropriate result list members were updated.
  static bool ParseSuggestResults(
      const base::Value::List& root_list,
      const AutocompleteInput& input,
      const AutocompleteSchemeClassifier& scheme_classifier,
      int default_result_relevance,
      bool is_keyword_result,
      Results* results);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCH_SUGGESTION_PARSER_H_
