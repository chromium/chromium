// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_id.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "third_party/omnibox_proto/chrome_searchbox_stats.pb.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

class TemplateURL;

// TemplateURLRef -------------------------------------------------------------

// A TemplateURLRef represents a single URL within the larger TemplateURL class
// (which represents an entire "search engine", see below).  If
// SupportsReplacement() is true, this URL has placeholders in it, for which
// callers can substitute values to get a "real" URL using ReplaceSearchTerms().
//
// TemplateURLRefs always have a non-NULL |owner_| TemplateURL, which they
// access in order to get at important data like the underlying URL string or
// the associated Profile.
class TemplateURLRef {
 public:
  // Magic numbers to pass to ReplaceSearchTerms() for the |accepted_suggestion|
  // parameter.  Most callers aren't using Suggest capabilities and should just
  // pass NO_SUGGESTIONS_AVAILABLE.
  // NOTE: Because positive values are meaningful, make sure these are negative!
  enum AcceptedSuggestion {
    NO_SUGGESTION_CHOSEN = -1,
    NO_SUGGESTIONS_AVAILABLE = -2,
  };

  // Which kind of URL within our owner we are.  This allows us to get at the
  // correct string field. Use |INDEXED| to indicate that the numerical
  // |index_in_owner_| should be used instead.
  enum Type {
    SEARCH,
    SUGGEST,
    IMAGE,
    IMAGE_TRANSLATE,
    NEW_TAB,
    CONTEXTUAL_SEARCH,
    INDEXED
  };

  using RequestSource = SearchTermsData::RequestSource;

  // Type to store <content_type, post_data> pair for POST URLs.
  // The |content_type|(first part of the pair) is the content-type of
  // the |post_data|(second part of the pair) which is encoded in
  // "multipart/form-data" format, it also contains the MIME boundary used in
  // the |post_data|. See http://tools.ietf.org/html/rfc2046 for the details.
  typedef std::pair<std::string, std::string> PostContent;

  // This struct encapsulates arguments passed to
  // TemplateURLRef::ReplaceSearchTerms methods.  By default, only search_terms
  // is required and is passed in the constructor.
  struct SearchTermsArgs {
    SearchTermsArgs();
    explicit SearchTermsArgs(const std::u16string& search_terms);
    SearchTermsArgs(const SearchTermsArgs& other);
    ~SearchTermsArgs();

    struct ContextualSearchParams {
      ContextualSearchParams();
      // Modern constructor, used when the content is sent in the HTTP header
      // instead of as CGI parameters.
      // The |version| tell the server which version of the client is making
      // this request.
      // The |contextual_cards_version| tells the server which version of
      // contextual cards integration is being used by the client.
      // The |home_country| is an ISO country code for the country that the user
      // considers their permanent home (which may be different from the country
      // they are currently visiting).  Pass an empty string if none available.
      // The |previous_event_id| is an identifier previously returned by the
      // server to identify that user interaction.
      // The |previous_event_results| are the results of the user-interaction of
      // that previous request.
      // The "previous_xyz" parameters are documented in go/cs-sanitized.
      // The |is_exact_search| allows the search request to be narrowed down to
      // an "exact" search only, meaning just search for X rather than X +
      // whatever else is in the context.  The returned search term should not
      // be expanded, and the server will honor this along with creating a
      // narrow Search Term.
      // The |source_lang| specifies a source language hint to apply for
      // translation or to indicate that translation might be appropriate.
      // This comes from CLD evaluating the selection and/or page content.
      // The |target_lang| specifies the best language to translate into for
      // the user, which also indicates when translation is appropriate or
      // helpful.  This comes from the Chrome Language Model.
      // The |fluent_languages| string specifies the languages the user
      // is fluent in reading.  This acts as an alternate set of languages
      // to consider translating into.  The languages are ordered by
      // fluency, and encoded as a comma-separated list of BCP 47 languages.
      // The |related_searches_stamp| string contains an information that
      // indicates experiment status and server processing results so that
      // can be logged in GWS Sawmill logs for offline analysis for the
      // Related Searches MVP experiment.
      // The |apply_lang_hint| specifies whether or not the |source_lang| should
      // be used as a hint for backend language detection. Otherwise, backend
      // translation is forced using |source_lang|. Note that this only supports
      // Partial Translate and so may only be enabled for select clients on the
      // server.
      ContextualSearchParams(int version,
                             int contextual_cards_version,
                             std::string home_country,
                             int64_t previous_event_id,
                             int previous_event_results,
                             bool is_exact_search,
                             std::string source_lang,
                             std::string target_lang,
                             std::string fluent_languages,
                             std::string related_searches_stamp,
                             bool apply_lang_hint);
      ContextualSearchParams(const ContextualSearchParams& other);
      ~ContextualSearchParams();

      // Estimates dynamic memory usage.
      // See base/trace_event/memory_usage_estimator.h for more info.
      size_t EstimateMemoryUsage() const;

      // The version of contextual search.
      int version = -1;

      // The version of Contextual Cards data to request.
      // A value of 0 indicates no data needed.
      int contextual_cards_version = 0;

      // The locale of the user's home country in an ISO country code format,
      // or an empty string if not available.  This indicates where the user
      // resides, not where they currently are.
      std::string home_country;

      // An EventID from a previous interaction (sent by server, recorded by
      // client).
      int64_t previous_event_id = 0l;

      // An encoded set of booleans that represent the interaction results from
      // the previous event.
      int previous_event_results = 0;

      // A flag that restricts the search to exactly match the selection rather
      // than expanding the Search Term to include other words in the context.
      bool is_exact_search = false;

      // Source language string to translate from.
      std::string source_lang;

      // Target language string to be translated into.
      std::string target_lang;

      // Alternate target languages that the user is fluent in, encoded in a
      // single string.
      std::string fluent_languages;

      // Experiment arm and processing information for the Related Searches
      // experiment. The value is an arbitrary string that starts with a
      // schema version number.
      std::string related_searches_stamp;

      // Whether hinted language detection should be used on the backend.
      bool apply_lang_hint = false;
    };

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;

    // The search terms (query).
    std::u16string search_terms;

    // The original (input) query.
    std::u16string original_query;

    // The type the original input query was identified as.
    metrics::OmniboxInputType input_type = metrics::OmniboxInputType::EMPTY;

    // Specifies how the user last interacted with the searchbox UI element.
    metrics::OmniboxFocusType focus_type =
        metrics::OmniboxFocusType::INTERACTION_DEFAULT;

    // The optional searchbox stats, reported as gs_lcrp for logging purposes.
    // This proto message contains information such as impressions of all
    // autocomplete matches shown at the query submission time.
    // For privacy reasons, we require the search provider to support HTTPS
    // protocol in order to receive the gs_lcrp param.
    // For more details, see go/chrome-suggest-logging-improvement.
    omnibox::metrics::ChromeSearchboxStats searchbox_stats;

    // TODO: Remove along with "aq" CGI param.
    int accepted_suggestion = NO_SUGGESTIONS_AVAILABLE;

    // The 0-based position of the cursor within the query string at the time
    // the request was issued.  Set to std::u16string::npos if not used.
    size_t cursor_position = std::u16string::npos;

    // The URL of the current webpage.
    std::string current_page_url;

    // The lens overlay suggest inputs to be sent in query parameters in
    // the suggest requests.
    std::optional<lens::proto::LensOverlaySuggestInputs>
        lens_overlay_suggest_inputs;

    // Which omnibox the user used to type the prefix.
    metrics::OmniboxEventProto::PageClassification page_classification =
        metrics::OmniboxEventProto::INVALID_SPEC;

    // Optional session token.
    std::string session_token;

    // Prefetch query and type.
    std::string prefetch_query;
    std::string prefetch_query_type;

    // Additional query params to append to the request.
    std::string additional_query_params;

    // If set, ReplaceSearchTerms() will automatically append any extra query
    // params specified via the --extra-search-query-params command-line
    // argument.  Generally, this should be set when dealing with the search
    // TemplateURLRefs of the default search engine and the caller cares
    // about the query portion of the URL.  Since neither TemplateURLRef nor
    // indeed TemplateURL know whether a TemplateURL is the default search
    // engine, callers instead must set this manually.
    bool append_extra_query_params_from_command_line = false;

    // The raw content of an image thumbnail that will be used as a query for
    // search-by-image frontend.
    std::string image_thumbnail_content;

    // The content type string for `image_thumbnail_content`.
    std::string image_thumbnail_content_type;

    // The image dimension data for a Google search-by-image query.
    std::string processed_image_dimensions;

    // When searching for an image, the URL of the original image. Callers
    // should leave this empty for images specified via data: URLs.
    GURL image_url;

    // When searching for an image, the original size of the image.
    gfx::Size image_original_size;

    // Source of the search or suggest request.
    RequestSource request_source = RequestSource::SEARCHBOX;

    // When the query is being fetched as a prefetch request, this is the value
    // corresponding to the GOOGLE_PREFETCH_SOURCE ("pf") query param. Prefetch
    // query params are not added if this is an empty string.
    std::string prefetch_param;

    ContextualSearchParams contextual_search_params;

    // The cache duration to be sent as a query string parameter in the zero
    // suggest requests, if non-zero.
    uint32_t zero_suggest_cache_duration_sec = 0;

    // Whether the request should bypass the HTTP cache, i.e., a "shift-reload".
    // If true, the net::LOAD_BYPASS_CACHE load flag will be set on the request.
    bool bypass_cache = false;

    // The source locale used for image translations.
    std::string image_translate_source_locale;

    // The target locale used for image translations.
    std::string image_translate_target_locale;
  };

  TemplateURLRef(const TemplateURL* owner, Type type);
  TemplateURLRef(const TemplateURL* owner, size_t index_in_owner);
  ~TemplateURLRef();

  TemplateURLRef(const TemplateURLRef& source);
  TemplateURLRef& operator=(const TemplateURLRef& source);

  // Returns the raw URL. None of the parameters will have been replaced.
  std::string GetURL() const;

  // Returns the raw string of the post params. Please see comments in
  // prepopulated_engines_schema.json for the format.
  std::string GetPostParamsString() const;

  // Returns true if this URL supports search term replacement.
  bool SupportsReplacement(const SearchTermsData& search_terms_data) const;

  // Returns a string that is the result of replacing the search terms in
  // the url with the specified arguments.  We use our owner's input encoding.
  //
  // If this TemplateURLRef does not support replacement (SupportsReplacement
  // returns false), an empty string is returned.
  // If this TemplateURLRef uses POST, and `post_content` is not NULL, the
  // `post_params_` will be replaced, encoded in "multipart/form-data" format
  // and stored into `post_content`.
  //
  // If `url_override` is set to a valid url, that url will be used and the url
  // in the TemplateURL will be disregarded.  This is currently used to allow
  // setting the URL of the @gemini scope for pre-prod testing without modifying
  // any in-memory or database entries.
  // TODO(crbug.com/41494524): Remove the `url_override` when the
  //  `StarterPackExpansion` feature launches/gets cleaned up.
  std::string ReplaceSearchTerms(const SearchTermsArgs& search_terms_args,
                                 const SearchTermsData& search_terms_data,
                                 PostContent* post_content,
                                 std::string url_override = "") const;

  // TODO(jnd): remove the following ReplaceSearchTerms definition which does
  // not have `post_content` parameter once all reference callers pass
  // `post_content` parameter.
  //
  // TODO(crbug.com/41494524): Remove the `url_override` when the
  //  `StarterPackExpansion` feature launches/gets cleaned up.
  std::string ReplaceSearchTerms(const SearchTermsArgs& search_terms_args,
                                 const SearchTermsData& search_terms_data,
                                 std::string url_override = "") const {
    return ReplaceSearchTerms(search_terms_args, search_terms_data, nullptr,
                              url_override);
  }

  // Returns true if the TemplateURLRef is valid. An invalid TemplateURLRef is
  // one that contains unknown terms, or invalid characters.
  bool IsValid(const SearchTermsData& search_terms_data) const;

  // Returns a string representation of this TemplateURLRef suitable for
  // display. The display format is the same as the format used by Firefox.
  std::u16string DisplayURL(const SearchTermsData& search_terms_data) const;

  // Converts a string as returned by DisplayURL back into a string as
  // understood by TemplateURLRef.
  static std::string DisplayURLToURLRef(const std::u16string& display_url);

  // If this TemplateURLRef is valid and contains one search term, this returns
  // the host/path of the URL, otherwise this returns an empty string.
  const std::string& GetHost(const SearchTermsData& search_terms_data) const;
  std::string GetPath(const SearchTermsData& search_terms_data) const;

  // If this TemplateURLRef is valid and contains one search term
  // in its query or ref, this returns the key of the search term,
  // otherwise this returns an empty string.
  const std::string& GetSearchTermKey(
      const SearchTermsData& search_terms_data) const;

  // If this TemplateURLRef is valid and contains one search term,
  // this returns the location of the search term,
  // otherwise this returns url::Parsed::QUERY.
  url::Parsed::ComponentType GetSearchTermKeyLocation(
      const SearchTermsData& search_terms_data) const;

  // If this TemplateURLRef is valid and contains one search term,
  // this returns the fixed prefix before the search term,
  // otherwise this returns an empty string.
  const std::string& GetSearchTermValuePrefix(
      const SearchTermsData& search_terms_data) const;

  // If this TemplateURLRef is valid and contains one search term,
  // this returns the fixed suffix after the search term,
  // otherwise this returns an empty string.
  const std::string& GetSearchTermValueSuffix(
      const SearchTermsData& search_terms_data) const;

  // Converts the specified term in our owner's encoding to a std::u16string.
  std::u16string SearchTermToString16(std::string_view term) const;

  // Returns true if this TemplateURLRef has a replacement term of
  // {google:baseURL} or {google:baseSuggestURL}.
  bool HasGoogleBaseURLs(const SearchTermsData& search_terms_data) const;

  // Use the pattern referred to by this TemplateURLRef to match the provided
  // |url| and extract |search_terms| from it. Returns true if the pattern
  // matches, even if |search_terms| is empty. In this case
  // |search_term_component|, if not NULL, indicates whether the search terms
  // were found in the query or the ref parameters; and |search_terms_position|,
  // if not NULL, contains the position of the search terms in the query or the
  // ref parameters. Returns false and an empty |search_terms| if the pattern
  // does not match.
  bool ExtractSearchTermsFromURL(
      const GURL& url,
      std::u16string* search_terms,
      const SearchTermsData& search_terms_data,
      url::Parsed::ComponentType* search_term_component,
      url::Component* search_terms_position) const;

  // Whether the URL uses POST (as opposed to GET).
  bool UsesPOSTMethod(const SearchTermsData& search_terms_data) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  friend class TemplateURL;
  friend class TemplateURLTest;
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest,
                           ImageThumbnailContentTypePostParams);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, SetPrepopulatedAndParse);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseParameterKnown);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseParameterUnknown);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLEmpty);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNoTemplateEnd);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNoKnownParameters);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLTwoParameters);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNestedParameter);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParsePlayStoreDefinitions);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, URLRefTestImageURLWithPOST);

  // Enumeration of the known types.
  enum ReplacementType {
    ENCODING,
    GOOGLE_ASSISTED_QUERY_STATS,
    GOOGLE_BASE_SEARCH_BY_IMAGE_URL,
    GOOGLE_BASE_SUGGEST_URL,
    GOOGLE_BASE_URL,
    GOOGLE_CLIENT_CACHE_TIME_TO_LIVE,
    GOOGLE_CONTEXTUAL_SEARCH_CONTEXT_DATA,
    GOOGLE_CONTEXTUAL_SEARCH_VERSION,
    GOOGLE_CURRENT_PAGE_URL,
    GOOGLE_CURSOR_POSITION,
    GOOGLE_IMAGE_ORIGINAL_HEIGHT,
    GOOGLE_IMAGE_ORIGINAL_WIDTH,
    GOOGLE_IMAGE_SEARCH_SOURCE,
    GOOGLE_IMAGE_THUMBNAIL_BASE64,
    GOOGLE_IMAGE_THUMBNAIL,
    GOOGLE_IMAGE_URL,
    GOOGLE_INPUT_TYPE,
    GOOGLE_LANGUAGE,
    GOOGLE_NTP_IS_THEMED,
    GOOGLE_OMNIBOX_FOCUS_TYPE,
    GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
    GOOGLE_PAGE_CLASSIFICATION,
    GOOGLE_PREFETCH_QUERY,
    GOOGLE_PREFETCH_SOURCE,
    GOOGLE_PROCESSED_IMAGE_DIMENSIONS,
    GOOGLE_RLZ,
    GOOGLE_SEARCH_CLIENT,
    GOOGLE_SEARCH_FIELDTRIAL_GROUP,
    GOOGLE_SEARCH_SOURCE_ID,
    GOOGLE_SEARCH_VERSION,
    GOOGLE_SESSION_TOKEN,
    GOOGLE_SUGGEST_CLIENT,
    GOOGLE_SUGGEST_REQUEST_ID,
    GOOGLE_UNESCAPED_SEARCH_TERMS,
    LANGUAGE,
    MAIL_RU_REFERRAL_ID,
    SEARCH_TERMS,
    YANDEX_REFERRAL_ID,
    IMAGE_TRANSLATE_SOURCE_LOCALE,
    IMAGE_TRANSLATE_TARGET_LOCALE
  };

  // Used to identify an element of the raw url that can be replaced.
  struct Replacement {
    Replacement(ReplacementType type, size_t index)
        : type(type), index(index), is_post_param(false) {}
    ReplacementType type;
    size_t index;
    // Indicates the location in where the replacement is replaced. If
    // |is_post_param| is false, |index| indicates the byte position in
    // |parsed_url_|. Otherwise, |index| is the index of |post_params_|.
    bool is_post_param;
  };

  // Stores a single parameter for a POST.
  struct PostParam {
    std::string name;
    std::string value;
    std::string content_type;

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;
  };

  // The list of elements to replace.
  typedef std::vector<struct Replacement> Replacements;
  typedef std::vector<PostParam> PostParams;

  // TemplateURLRef internally caches values to make replacement quick. This
  // method invalidates any cached values.
  void InvalidateCachedValues() const;

  // Parses the parameter in url at the specified offset. start/end specify the
  // range of the parameter in the url, including the braces. If the parameter
  // is valid, url is updated to reflect the appropriate parameter. If
  // the parameter is one of the known parameters an element is added to
  // replacements indicating the type and range of the element. The original
  // parameter is erased from the url.
  //
  // If the parameter is not a known parameter, false is returned. If this is a
  // prepopulated URL, the parameter is erased, otherwise it is left alone.
  bool ParseParameter(size_t start,
                      size_t end,
                      std::string* url,
                      Replacements* replacements) const;

  // Parses the specified url, replacing parameters as necessary. If
  // successful, valid is set to true, and the parsed url is returned. For all
  // known parameters that are encountered an entry is added to replacements.
  // If there is an error parsing the url, valid is set to false, and an empty
  // string is returned.  If the URL has the POST parameters, they will be
  // parsed into |post_params| which will be further replaced with real search
  // terms data and encoded in "multipart/form-data" format to generate the
  // POST data.
  std::string ParseURL(const std::string& url,
                       Replacements* replacements,
                       PostParams* post_params,
                       bool* valid) const;

  // If the url has not yet been parsed, ParseURL is invoked.
  // NOTE: While this is const, it modifies parsed_, valid_, parsed_url_ and
  // search_offset_.
  //
  // TODO(crbug.com/41494524): Remove the `url_override` when the
  //  `StarterPackExpansion` feature launches/gets cleaned up.
  void ParseIfNecessary(const SearchTermsData& search_terms_data,
                        std::string url_override = "") const;

  // Parses a wildcard out of |path|, putting the parsed path in |path_prefix_|
  // and |path_suffix_| and setting |path_wildcard_present_| to true.
  // In the absence of a wildcard, the full path will be contained in
  // |path_prefix_| and |path_wildcard_present_| will be false.
  void ParsePath(const std::string& path) const;

  // Returns whether the path portion of this template URL is equal to the path
  // in |url|, checking that URL is prefixed/suffixed by
  // |path_prefix_|/|path_suffix_| if |path_wildcard_present_| is true, or equal
  // to |path_prefix_| otherwise.
  bool PathIsEqual(const GURL& url) const;

  // Extracts the query key and host from the url.
  void ParseHostAndSearchTermKey(
      const SearchTermsData& search_terms_data) const;

  // Encode post parameters in "multipart/form-data" format and store it
  // inside |post_content|. Returns false if errors are encountered during
  // encoding. This method is called each time ReplaceSearchTerms gets called.
  bool EncodeFormData(const PostParams& post_params,
                      PostContent* post_content) const;

  // Handles a replacement by using real term data. If the replacement
  // belongs to a PostParam, the PostParam will be replaced by the term data.
  // Otherwise, the term data will be inserted at the place that the
  // replacement points to.
  // Can be called repeatedly with the same replacement.
  void HandleReplacement(const std::string& name,
                         const std::string& value,
                         const Replacement& replacement,
                         std::string* url) const;

  // Replaces all replacements in |parsed_url_| with their actual values and
  // returns the result.  This is the main functionality of
  // ReplaceSearchTerms().
  std::string HandleReplacements(const SearchTermsArgs& search_terms_args,
                                 const SearchTermsData& search_terms_data,
                                 PostContent* post_content) const;

  // The TemplateURL that contains us.  This should outlive us.
  raw_ptr<const TemplateURL> owner_;

  // What kind of URL we are.
  Type type_;

  // If |type_| is |INDEXED|, this |index_in_owner_| is used instead to refer to
  // a url within our owner.
  size_t index_in_owner_ = 0;

  // Whether the URL has been parsed.
  mutable bool parsed_ = false;

  // Whether the url was successfully parsed.
  mutable bool valid_ = false;

  // The parsed URL. All terms have been stripped out of this with
  // replacements_ giving the index of the terms to replace.
  mutable std::string parsed_url_;

  // Do we support search term replacement?
  mutable bool supports_replacements_ = false;

  // The replaceable parts of url (parsed_url_). These are ordered by index
  // into the string, and may be empty.
  mutable Replacements replacements_;

  // Whether the path contains a wildcard.
  mutable bool path_wildcard_present_ = false;

  // Host, port, path, key and location of the search term. These are only set
  // if the url contains one search term.
  mutable std::string host_;
  mutable std::string port_;
  mutable std::string path_prefix_;
  mutable std::string path_suffix_;
  mutable std::string search_term_key_;
  mutable url::Parsed::ComponentType search_term_key_location_ =
      url::Parsed::QUERY;
  mutable std::string search_term_value_prefix_;
  mutable std::string search_term_value_suffix_;

  mutable PostParams post_params_;

  // Whether the contained URL is a pre-populated URL.
  bool prepopulated_ = false;
};

// TemplateURL ----------------------------------------------------------------

// A TemplateURL represents a single "search engine", defined primarily as a
// subset of the Open Search Description Document
// (http://www.opensearch.org/Specifications/OpenSearch) plus some extensions.
// One TemplateURL contains several TemplateURLRefs, which correspond to various
// different capabilities (e.g. doing searches or getting suggestions), as well
// as a TemplateURLData containing other details like the name, keyword, etc.
//
// TemplateURLs are intended to be read-only for most users.
// The TemplateURLService, which handles storing and manipulating TemplateURLs,
// is made a friend so that it can be the exception to this pattern.
class TemplateURL {
 public:
  using TemplateURLVector =
      std::vector<raw_ptr<TemplateURL, VectorExperimental>>;
  using OwnedTemplateURLVector = std::vector<std::unique_ptr<TemplateURL>>;

  // These values are not persisted and can be freely changed.
  // Their integer values are used for choosing the best engine during keyword
  // conflicts, so their relative ordering should not be changed without careful
  // thought about what happens during version skew.
  enum Type {
    // Installed only on this device. Should not be synced. This is not common.
    LOCAL = 0,
    // Regular search engine. This is the most common, and the ONLY type synced.
    NORMAL = 1,
    // Installed by extension through Override Settings API. Not synced.
    NORMAL_CONTROLLED_BY_EXTENSION = 2,
    // The keyword associated with an extension that uses the Omnibox API.
    // Not synced.
    OMNIBOX_API_EXTENSION = 3,
  };

  // An AssociatedExtensionInfo represents information about the extension that
  // added the search engine.
  struct AssociatedExtensionInfo {
    AssociatedExtensionInfo(const std::string& extension_id,
                            base::Time install_time,
                            bool wants_to_be_default_engine);
    ~AssociatedExtensionInfo();

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;

    std::string extension_id;

    // Used to resolve conflicts when there are multiple extensions specifying
    // the default search engine. The most recently-installed wins.
    base::Time install_time;

    // Whether the search engine is supposed to be default.
    bool wants_to_be_default_engine;
  };

  explicit TemplateURL(const TemplateURLData& data, Type type = NORMAL);

  // Constructor for extension controlled engine. |type| must be
  // NORMAL_CONTROLLED_BY_EXTENSION or OMNIBOX_API_EXTENSION.
  TemplateURL(const TemplateURLData& data,
              Type type,
              std::string extension_id,
              base::Time install_time,
              bool wants_to_be_default_engine);

  TemplateURL(const TemplateURL&) = delete;
  TemplateURL& operator=(const TemplateURL&) = delete;

  ~TemplateURL();

  // For two engines, |this| and |other|, returns true if |this| is strictly
  // better than |other|.
  //
  // While normal engines must all have distinct keywords, policy-created,
  // extension-controlled and omnibox API engines may have the same keywords as
  // each other or as normal engines.  In these cases, policy-create engines
  // override omnibox API engines, which override extension-controlled engines,
  // which override normal engines.
  //
  // If there is still a conflict after this, compare by safe-for-autoreplace,
  // then last modified date, then use the sync guid as a tiebreaker.
  //
  // TODO(tommycli): I'd like to use this to resolve Sync conflicts in the
  // future, but we need a total ordering of TemplateURLs. That's not the case
  // today, because the sync GUIDs are not actually globally unique, so there
  // can be a genuine tie, which is not good, because then two different clients
  // could choose to resolve the conflict in two different ways.
  bool IsBetterThanConflictingEngine(const TemplateURL* other) const;

  // Generates a suitable keyword for the specified url, which must be valid.
  // This is guaranteed not to return an empty string, since TemplateURLs should
  // never have an empty keyword.
  static std::u16string GenerateKeyword(const GURL& url);

  // Generates a favicon URL from the specified url.
  static GURL GenerateFaviconURL(const GURL& url);

  // Returns true if |t_url| and |data| are equal in all meaningful respects.
  // Static to allow either or both params to be NULL.
  static bool MatchesData(const TemplateURL* t_url,
                          const TemplateURLData* data,
                          const SearchTermsData& search_terms_data);

  const TemplateURLData& data() const { return data_; }

  const std::u16string& short_name() const { return data_.short_name(); }
  // An accessor for the short_name, but adjusted so it can be appropriately
  // displayed even if it is LTR and the UI is RTL.
  std::u16string AdjustedShortNameForLocaleDirection() const;

  const std::u16string& keyword() const { return data_.keyword(); }

  const std::string& url() const { return data_.url(); }
  const std::string& suggestions_url() const { return data_.suggestions_url; }
  const std::string& image_url() const { return data_.image_url; }
  const std::string& image_translate_url() const {
    return data_.image_translate_url;
  }
  const std::string& new_tab_url() const { return data_.new_tab_url; }
  const std::string& contextual_search_url() const {
    return data_.contextual_search_url;
  }
  const std::string& search_url_post_params() const {
    return data_.search_url_post_params;
  }
  const std::string& suggestions_url_post_params() const {
    return data_.suggestions_url_post_params;
  }
  const std::string& image_url_post_params() const {
    return data_.image_url_post_params;
  }
  const std::string& side_search_param() const {
    return data_.side_search_param;
  }
  const std::string& side_image_search_param() const {
    return data_.side_image_search_param;
  }
  const std::string& image_translate_source_language_param_key() const {
    return data_.image_translate_source_language_param_key;
  }
  const std::string& image_translate_target_language_param_key() const {
    return data_.image_translate_target_language_param_key;
  }
  const std::u16string& image_search_branding_label() const {
    return !data_.image_search_branding_label.empty()
               ? data_.image_search_branding_label
               : short_name();
  }
  const std::vector<std::string>& search_intent_params() const {
    return data_.search_intent_params;
  }
  const std::vector<std::string>& alternate_urls() const {
    return data_.alternate_urls;
  }
  const GURL& favicon_url() const { return data_.favicon_url; }

  const GURL& logo_url() const { return data_.logo_url; }

  const GURL& doodle_url() const { return data_.doodle_url; }

  const GURL& originating_url() const { return data_.originating_url; }

  bool safe_for_autoreplace() const { return data_.safe_for_autoreplace; }

  const std::vector<std::string>& input_encodings() const {
    return data_.input_encodings;
  }

  TemplateURLID id() const { return data_.id; }

  base::Time date_created() const { return data_.date_created; }
  base::Time last_modified() const { return data_.last_modified; }
  base::Time last_visited() const { return data_.last_visited; }

  TemplateURLData::CreatedByPolicy created_by_policy() const {
    return data_.created_by_policy;
  }
  bool enforced_by_policy() const { return data_.enforced_by_policy; }
  bool created_from_play_api() const { return data_.created_from_play_api; }
  bool featured_by_policy() const { return data_.featured_by_policy; }

  int usage_count() const { return data_.usage_count; }

  int prepopulate_id() const { return data_.prepopulate_id; }

  const std::string& sync_guid() const { return data_.sync_guid; }

  TemplateURLData::ActiveStatus is_active() const { return data_.is_active; }

  int starter_pack_id() const { return data_.starter_pack_id; }

  const std::vector<TemplateURLRef>& url_refs() const { return url_refs_; }
  const TemplateURLRef& url_ref() const {
    // Sanity check for https://crbug.com/781703.
    CHECK(!url_refs_.empty());
    return url_refs_.back();
  }
  const TemplateURLRef& suggestions_url_ref() const {
    return suggestions_url_ref_;
  }
  const TemplateURLRef& image_url_ref() const { return image_url_ref_; }
  const TemplateURLRef& image_translate_url_ref() const {
    return image_translate_url_ref_;
  }
  const TemplateURLRef& new_tab_url_ref() const { return new_tab_url_ref_; }
  const TemplateURLRef& contextual_search_url_ref() const {
    return contextual_search_url_ref_;
  }

  Type type() const { return type_; }

  const AssociatedExtensionInfo* GetExtensionInfoForTesting() const {
    return extension_info_.get();
  }

  // Returns true if |url| supports replacement.
  bool SupportsReplacement(const SearchTermsData& search_terms_data) const;

  // Returns true if any URLRefs use Googe base URLs.
  bool HasGoogleBaseURLs(const SearchTermsData& search_terms_data) const;

  // Returns true if this TemplateURL uses Google base URLs and has a keyword
  // of "google.TLD".  We use this to decide whether we can automatically
  // update the keyword to reflect the current Google base URL TLD.
  bool IsGoogleSearchURLWithReplaceableKeyword(
      const SearchTermsData& search_terms_data) const;

  // Returns true if the keywords match or if
  // IsGoogleSearchURLWithReplaceableKeyword() is true for both |this| and
  // |other|.
  bool HasSameKeywordAs(const TemplateURLData& other,
                        const SearchTermsData& search_terms_data) const;

  // Returns the id of the extension that added this search engine. Only call
  // this for TemplateURLs of type NORMAL_CONTROLLED_BY_EXTENSION or
  // OMNIBOX_API_EXTENSION.
  std::string GetExtensionId() const;

  // Returns the type of this search engine, or SEARCH_ENGINE_OTHER if no
  // engines match.
  SearchEngineType GetEngineType(
      const SearchTermsData& search_terms_data) const;

  // Returns the type of this search engine, i.e. whether the engine is a
  // prepopulated engine, starter pack engine, or not built-in.
  BuiltinEngineType GetBuiltinEngineType() const;

  // Use the alternate URLs and the search URL to match the provided |url|
  // and extract |search_terms| from it. Returns false and an empty
  // |search_terms| if no search terms can be matched. The URLs are matched in
  // the order listed in |url_refs_| (see comment there).
  bool ExtractSearchTermsFromURL(const GURL& url,
                                 const SearchTermsData& search_terms_data,
                                 std::u16string* search_terms) const;

  // Returns true if non-empty search terms could be extracted from |url| using
  // ExtractSearchTermsFromURL(). In other words, this returns whether |url|
  // could be the result of performing a search with |this|.
  bool IsSearchURL(const GURL& url,
                   const SearchTermsData& search_terms_data) const;

  // Given a `url` corresponding to this TemplateURL, keeps the search terms and
  // optionally the search intent params and removes the other params. If
  // `normalize_search_terms` is true, the search terms in the final URL
  // will be converted to lowercase with extra whitespace characters collapsed.
  // If `url` is not a search URL or replacement fails, leaves `out_url` and
  // `out_search_terms` untouched and returns false. Used to compare
  // normalized (aka canonical) search URLs.
  bool KeepSearchTermsInURL(const GURL& url,
                            const SearchTermsData& search_terms_data,
                            const bool keep_search_intent_params,
                            const bool normalize_search_terms,
                            GURL* out_url,
                            std::u16string* out_search_terms = nullptr) const;

  // Given a |url| corresponding to this TemplateURL, identifies the search
  // terms and replaces them with the ones in |search_terms_args|, leaving the
  // other parameters untouched. If the replacement fails, returns false and
  // leaves |result| untouched. This is used by mobile ports to perform query
  // refinement.
  bool ReplaceSearchTermsInURL(
      const GURL& url,
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      const SearchTermsData& search_terms_data,
      GURL* result) const;

  // Encodes the search terms from |search_terms_args| so that we know the
  // |input_encoding|. Returns the |encoded_terms| and the
  // |encoded_original_query|. |encoded_terms| may be escaped as path or query
  // depending on |is_in_query|; |encoded_original_query| is always escaped as
  // query.
  void EncodeSearchTerms(
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      bool is_in_query,
      std::string* input_encoding,
      std::u16string* encoded_terms,
      std::u16string* encoded_original_query) const;

  // Returns the search url for this template URL and the optional search terms.
  // Uses something obscure as the default value for the search terms argument
  // so that in the rare case the term replaces the URL it's unlikely another
  // keyword would have the same url.
  // Returns an empty GURL if this template URL has no url().
  GURL GenerateSearchURL(
      const SearchTermsData& search_terms_data,
      const std::u16string& search_terms = u"blah.blah.blah.blah.blah") const;

  // Returns the suggest endpoint URL for this template URL.
  // Returns an empty GURL if this template URL has no suggestions_url().
  GURL GenerateSuggestionURL(const SearchTermsData& search_terms_data) const;

  // Returns true if this search engine supports the side search feature.
  bool IsSideSearchSupported() const;

  // Returns true if this search engine supports the side image search feature.
  bool IsSideImageSearchSupported() const;

  // Takes a search URL belonging to this search engine and generates the URL
  // appropriate for the side search side panel.
  GURL GenerateSideSearchURL(const GURL& search_url,
                             const std::string& version,
                             const SearchTermsData& search_terms_data) const;

  // Takes a search URL that belongs to this side search in the side panel and
  // removes the side search param from the URL.
  GURL RemoveSideSearchParamFromURL(const GURL& side_search_url) const;

  // Takes a search URL belonging to this image search engine and generates the
  // URL appropriate for the image search in the side panel.
  GURL GenerateSideImageSearchURL(const GURL& image_search_url,
                                  const std::string& version) const;

  // Takes a search URL that belongs to this image search in the side panel and
  // removes the side image search param from the URL.
  GURL RemoveSideImageSearchParamFromURL(const GURL& image_search_url) const;

  // TemplateURL internally caches values derived from a passed SearchTermsData
  // to make its functions quick. This method invalidates any cached values and
  // it should be called after SearchTermsData has been changed.
  void InvalidateCachedValues() const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Returns whether |url| query contains a side search param.
  bool ContainsSideSearchParam(const GURL& url) const;

  // Returns whether |url| query contains a side image search param.
  bool ContainsSideImageSearchParam(const GURL& url) const;

  // Returns the RegulatoryExtensionType appropriate for this instance of the
  // TemplateURL.
  RegulatoryExtensionType GetRegulatoryExtensionType() const;

  // Returns the specific data associated with the supplied
  // RegulatoryExtensionType.
  const TemplateURLData::RegulatoryExtension* GetRegulatoryExtension(
      RegulatoryExtensionType type) const;

 private:
  friend class TemplateURLService;

  void CopyFrom(const TemplateURL& other);

  void SetURL(const std::string& url);
  void SetPrepopulateId(int id);

  // Resets the keyword if IsGoogleSearchURLWithReplaceableKeyword() or |force|.
  // The |force| parameter is useful when the existing keyword is known to be
  // a placeholder.  The resulting keyword is generated using
  // GenerateSearchURL() and GenerateKeyword().
  void ResetKeywordIfNecessary(const SearchTermsData& search_terms_data,
                               bool force);

  // Resizes the |url_refs_| vector, which always holds the search URL as the
  // last item.
  void ResizeURLRefVector();

  // Uses the alternate URLs and the search URL to match the provided |url|
  // and extract |search_terms| from it as well as the |search_terms_component|
  // (either REF or QUERY) and |search_terms_component| at which the
  // |search_terms| are found in |url|. See also ExtractSearchTermsFromURL().
  bool FindSearchTermsInURL(const GURL& url,
                            const SearchTermsData& search_terms_data,
                            std::u16string* search_terms,
                            url::Parsed::ComponentType* search_terms_component,
                            url::Component* search_terms_position) const;

  TemplateURLData data_;

  // Contains TemplateURLRefs corresponding to the alternate URLs and the search
  // URL, in priority order: the URL at index 0 is treated as the highest
  // priority and the primary search URL is treated as the lowest priority.  For
  // example, if a TemplateURL has alternate URL "http://foo/#q={searchTerms}"
  // and search URL "http://foo/?q={searchTerms}", and the URL to be decoded is
  // "http://foo/?q=a#q=b", the alternate URL will match first and the decoded
  // search term will be "b".  Note that since every TemplateURLRef has a
  // primary search URL, this vector is never empty.
  std::vector<TemplateURLRef> url_refs_;

  TemplateURLRef suggestions_url_ref_;
  TemplateURLRef image_url_ref_;
  TemplateURLRef image_translate_url_ref_;
  TemplateURLRef new_tab_url_ref_;
  TemplateURLRef contextual_search_url_ref_;
  std::unique_ptr<AssociatedExtensionInfo> extension_info_;

  const Type type_;

  // Caches the computed engine type across successive calls to GetEngineType().
  mutable SearchEngineType engine_type_;

  // TODO(sky): Add date last parsed OSD file.
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_H_
