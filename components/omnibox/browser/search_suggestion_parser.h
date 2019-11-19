// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCH_SUGGESTION_PARSER_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCH_SUGGESTION_PARSER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "url/gurl.h"

class AutocompleteInput;
class AutocompleteSchemeClassifier;

namespace base {
class Value;
}

namespace network {
class SimpleURLLoader;
}

class SearchSuggestionParser {
 public:
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
           int subtype_identifier,
           const std::string& deletion_url);
    Result(const Result& other);
    virtual ~Result();

    bool from_keyword() const { return from_keyword_; }

    const base::string16& match_contents() const { return match_contents_; }
    const ACMatchClassifications& match_contents_class() const {
      return match_contents_class_;
    }

    AutocompleteMatchType::Type type() const { return type_; }
    int subtype_identifier() const { return subtype_identifier_; }
    int relevance() const { return relevance_; }
    void set_relevance(int relevance) { relevance_ = relevance; }
    bool received_after_last_keystroke() const {
      return received_after_last_keystroke_;
    }
    void set_received_after_last_keystroke(
        bool received_after_last_keystroke) {
      received_after_last_keystroke_ = received_after_last_keystroke;
    }

    bool relevance_from_server() const { return relevance_from_server_; }
    void set_relevance_from_server(bool relevance_from_server) {
      relevance_from_server_ = relevance_from_server;
    }

    const std::string& deletion_url() const { return deletion_url_; }

    // Returns the default relevance value for this result (which may
    // be left over from a previous omnibox input) given the current
    // input and whether the current input caused a keyword provider
    // to be active.
    virtual int CalculateRelevance(const AutocompleteInput& input,
                                   bool keyword_provider_requested) const = 0;

   protected:
    // The contents to be displayed and its style info.
    base::string16 match_contents_;
    ACMatchClassifications match_contents_class_;

    // True if the result came from a keyword suggestion.
    bool from_keyword_;

    AutocompleteMatchType::Type type_;

    // Used to identify the specific source / type for suggestions by the
    // suggest server. See |result_subtype_identifier| in omnibox.proto for more
    // details.
    // The identifier 0 is reserved for cases where this specific type is unset.
    int subtype_identifier_;

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
  };

  class SuggestResult : public Result {
   public:
    SuggestResult(const base::string16& suggestion,
                  AutocompleteMatchType::Type type,
                  int subtype_identifier,
                  bool from_keyword,
                  int relevance,
                  bool relevance_from_server,
                  const base::string16& input_text);
    SuggestResult(const base::string16& suggestion,
                  AutocompleteMatchType::Type type,
                  int subtype_identifier,
                  const base::string16& match_contents,
                  const base::string16& match_contents_prefix,
                  const base::string16& annotation,
                  const std::string& additional_query_params,
                  const std::string& deletion_url,
                  const std::string& image_dominant_color,
                  const std::string& image_url,
                  bool from_keyword,
                  int relevance,
                  bool relevance_from_server,
                  bool should_prefetch,
                  const base::string16& input_text);
    SuggestResult(const SuggestResult& result);
    ~SuggestResult() override;

    SuggestResult& operator=(const SuggestResult& rhs);

    const base::string16& suggestion() const { return suggestion_; }
    const base::string16& match_contents_prefix() const {
      return match_contents_prefix_;
    }
    const base::string16& annotation() const { return annotation_; }
    const std::string& additional_query_params() const {
      return additional_query_params_;
    }

    void SetAnswer(const SuggestionAnswer& answer);
    const base::Optional<SuggestionAnswer>& answer() const { return answer_; }

    const std::string& image_dominant_color() const {
      return image_dominant_color_;
    }
    const std::string& image_url() const { return image_url_; }

    bool should_prefetch() const { return should_prefetch_; }

    // Fills in |match_contents_class_| to reflect how |match_contents_| should
    // be displayed and bolded against the current |input_text|.  If
    // |allow_bolding_all| is false and |match_contents_class_| would have all
    // of |match_contents_| bolded, do nothing.
    void ClassifyMatchContents(const bool allow_bolding_all,
                               const base::string16& input_text);

    // Result:
    int CalculateRelevance(const AutocompleteInput& input,
                           bool keyword_provider_requested) const override;

   private:
    // The search terms to be used for this suggestion.
    base::string16 suggestion_;

    // The contents to be displayed as prefix of match contents.
    // Used for tail suggestions to display a leading ellipsis (or some
    // equivalent character) to indicate omitted text.
    // Only used to pass this information to about:omnibox's "Additional Info".
    base::string16 match_contents_prefix_;

    // Optional annotation for the |match_contents_| for disambiguation.
    // This may be displayed in the autocomplete match contents, but is defined
    // separately to facilitate different formatting.
    base::string16 annotation_;

    // Optional additional parameters to be added to the search URL.
    std::string additional_query_params_;

    // Optional short answer to the input that produced this suggestion.
    base::Optional<SuggestionAnswer> answer_;

    // Optional image information. Used for entity suggestions. The dominant
    // color can be used to paint the image placeholder while fetching the
    // image.
    std::string image_dominant_color_;
    std::string image_url_;

    // Should this result be prefetched?
    bool should_prefetch_;
  };

  class NavigationResult : public Result {
   public:
    NavigationResult(const AutocompleteSchemeClassifier& scheme_classifier,
                     const GURL& url,
                     AutocompleteMatchType::Type type,
                     int subtype_identifier,
                     const base::string16& description,
                     const std::string& deletion_url,
                     bool from_keyword,
                     int relevance,
                     bool relevance_from_server,
                     const base::string16& input_text);
    NavigationResult(const NavigationResult& other);
    ~NavigationResult() override;

    const GURL& url() const { return url_; }
    const base::string16& description() const { return description_; }
    const ACMatchClassifications& description_class() const {
      return description_class_;
    }
    const base::string16& formatted_url() const { return formatted_url_; }

    // Fills in |match_contents_| and |match_contents_class_| to reflect how
    // the URL should be displayed and bolded against the current |input_text|.
    // If |allow_bolding_nothing| is false and |match_contents_class_| would
    // result in an entirely unbolded |match_contents_|, do nothing.
    void CalculateAndClassifyMatchContents(const bool allow_bolding_nothing,
                                           const base::string16& input_text);

    // Result:
    int CalculateRelevance(const AutocompleteInput& input,
                           bool keyword_provider_requested) const override;

   private:
    void ClassifyDescription(const base::string16& input_text);

    // The suggested url for navigation.
    GURL url_;

    // The properly formatted ("fixed up") URL string with equivalent meaning
    // to the one in |url_|.
    base::string16 formatted_url_;

    // The suggested navigational result description; generally the site name.
    base::string16 description_;
    ACMatchClassifications description_class_;
  };

  typedef std::vector<SuggestResult> SuggestResults;
  typedef std::vector<NavigationResult> NavigationResults;

  // A simple structure bundling most of the information (including
  // both SuggestResults and NavigationResults) returned by a call to
  // the suggest server.
  //
  // This has to be declared after the typedefs since it relies on some of them.
  struct Results {
    Results();
    ~Results();

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

    // If the relevance values of the results are from the server.
    bool relevances_from_server;

   private:
    DISALLOW_COPY_AND_ASSIGN(Results);
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
  static std::unique_ptr<base::Value> DeserializeJsonData(
      base::StringPiece json_data);

  // Parses results from the suggest server and updates the appropriate suggest
  // and navigation result lists in |results|. |is_keyword_result| indicates
  // whether the response was received from the keyword provider.
  // Returns whether the appropriate result list members were updated.
  static bool ParseSuggestResults(
      const base::Value& root_val,
      const AutocompleteInput& input,
      const AutocompleteSchemeClassifier& scheme_classifier,
      int default_result_relevance,
      bool is_keyword_result,
      Results* results);

  DISALLOW_COPY_AND_ASSIGN(SearchSuggestionParser);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCH_SUGGESTION_PARSER_H_
