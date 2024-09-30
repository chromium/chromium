// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_suggestion_parser.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_i18n.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_formatter/url_formatter.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/omnibox_proto/entity_info.pb.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "third_party/omnibox_proto/rich_suggest_template.pb.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

namespace {

// Converts a suggestion type name found in the JSON response to an equivalent
// omnibox::SuggestType enum value.
omnibox::SuggestType GetSuggestType(const std::string& type) {
  if (type == "CALCULATOR") {
    return omnibox::TYPE_CALCULATOR;
  }
  if (type == "ENTITY") {
    return omnibox::TYPE_ENTITY;
  }
  if (type == "TAIL") {
    return omnibox::TYPE_TAIL;
  }
  if (type == "PERSONALIZED_QUERY") {
    return omnibox::TYPE_PERSONALIZED_QUERY;
  }
  if (type == "PROFILE") {
    return omnibox::TYPE_PROFILE;
  }
  if (type == "NAVIGATION") {
    return omnibox::TYPE_NAVIGATION;
  }
  if (type == "PERSONALIZED_NAVIGATION") {
    return omnibox::TYPE_PERSONALIZED_NAVIGATION;
  }
  if (type == "CHROME_QUERY_TILES") {
    return omnibox::TYPE_CHROME_QUERY_TILES;
  }
  if (type == "CATEGORICAL_QUERY") {
    return omnibox::TYPE_CATEGORICAL_QUERY;
  }
  return omnibox::TYPE_QUERY;
}

// Converts an omnibox::SuggestType enum value to an equivalent
// AutocompleteMatchType::Type enum values.
AutocompleteMatchType::Type GetAutocompleteMatchType(
    omnibox::SuggestType suggest_type) {
  switch (suggest_type) {
    case omnibox::TYPE_CALCULATOR:
      return AutocompleteMatchType::CALCULATOR;
    case omnibox::TYPE_ENTITY:
      return AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
    case omnibox::TYPE_TAIL:
      return AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
    case omnibox::TYPE_PERSONALIZED_QUERY:
      return AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED;
    case omnibox::TYPE_PROFILE:
      return AutocompleteMatchType::SEARCH_SUGGEST_PROFILE;
    case omnibox::TYPE_NAVIGATION:
      return AutocompleteMatchType::NAVSUGGEST;
    case omnibox::TYPE_PERSONALIZED_NAVIGATION:
      return AutocompleteMatchType::NAVSUGGEST_PERSONALIZED;
    default: {
      // Use `ACMatchType::SEARCH_SUGGEST_ENTITY` for categorical suggestions.
      if (suggest_type == omnibox::TYPE_CATEGORICAL_QUERY &&
          base::FeatureList::IsEnabled(omnibox::kCategoricalSuggestions)) {
        return AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
      }
      return AutocompleteMatchType::SEARCH_SUGGEST;
    }
  }
}

// Convert the supplied Json::Value representation of list-of-lists-of-integers
// to a vector-of-vecrors-of-integers, containing (ideally) one vector of
// integers per match.
// The logic here does not validate if the length of top level vector is same as
// number of returned matches and will supply empty vector for any item that is
// either invalid or missing.
// The function will always return a valid and properly sized vector of vectors,
// equal in length to `expected_size`, even if the input `subtypes_list` is not
// valid.
std::vector<std::vector<int>> ParseMatchSubtypes(
    const base::Value::List* subtypes_list,
    size_t expected_size) {
  std::vector<std::vector<int>> result(expected_size);

  if (subtypes_list == nullptr) {
    return result;
  }

  if (!subtypes_list->empty() && subtypes_list->size() != expected_size) {
    LOG(WARNING) << "The length of reported subtypes (" << subtypes_list->size()
                 << ") does not match the expected length (" << expected_size
                 << ')';
  }

  const auto num_items = std::min(expected_size, subtypes_list->size());
  for (auto index = 0u; index < num_items; index++) {
    const auto& subtypes_item = (*subtypes_list)[index];
    // Permissive: ignore subtypes that are not in a form of a list.
    if (!subtypes_item.is_list())
      continue;

    const auto& subtype_list = subtypes_item.GetList();
    auto& result_subtypes = result[index];
    result_subtypes.reserve(subtype_list.size());

    for (const auto& subtype : subtype_list) {
      // Permissive: Skip over any item that is not an integer.
      if (!subtype.is_int())
        continue;
      result_subtypes.emplace_back(subtype.GetInt());
    }
  }

  return result;
}

std::string FindStringOrEmpty(const base::Value::Dict& value, std::string key) {
  auto* ptr = value.FindString(key);
  return ptr ? *ptr : "";
}

// The field number for the experiment stat type specified as an int
// in ExperimentStatsV2.
constexpr char kTypeIntFieldNumber[] = "4";
// The field number for the string value in ExperimentStatsV2.
constexpr char kStringValueFieldNumber[] = "2";

constexpr auto kReservedReservedGroupSectionsMap =
    base::MakeFixedFlatMap<int, omnibox::GroupSection>(
        {{0, omnibox::SECTION_REMOTE_ZPS_1},
         {1, omnibox::SECTION_REMOTE_ZPS_2},
         {2, omnibox::SECTION_REMOTE_ZPS_3},
         {3, omnibox::SECTION_REMOTE_ZPS_4},
         {4, omnibox::SECTION_REMOTE_ZPS_5},
         {5, omnibox::SECTION_REMOTE_ZPS_6},
         {6, omnibox::SECTION_REMOTE_ZPS_7},
         {7, omnibox::SECTION_REMOTE_ZPS_8},
         {8, omnibox::SECTION_REMOTE_ZPS_9},
         {9, omnibox::SECTION_REMOTE_ZPS_10}});

// Converts the given 0-based index of a group in the server response to a group
// section known to Chrome.
omnibox::GroupSection ChromeGroupSectionForRemoteGroupIndex(
    const int group_index) {
  if (base::Contains(kReservedReservedGroupSectionsMap, group_index)) {
    return kReservedReservedGroupSectionsMap.at(group_index);
  } else {
    // Return a default section if we don't have any reserved sections left.
    return omnibox::SECTION_DEFAULT;
  }
}

// Decodes a proto object from its serialized Base64 string representation.
template <typename T>
bool DecodeProtoFromBase64(const std::string* encoded_data, T& result_proto) {
  if (!encoded_data || encoded_data->empty()) {
    return false;
  }

  std::string decoded_data;
  if (!base::Base64Decode(*encoded_data, &decoded_data)) {
    return false;
  }

  if (decoded_data.empty()) {
    return false;
  }

  if (!result_proto.ParseFromString(decoded_data)) {
    return false;
  }

  return true;
}

// Format template image URLs that do not contain a scheme.
// The call to GetFormattedURL() will return the URL with a scheme added or
// return the same URL if no formatting is necessary.
void FormatAnswerTemplateImageURL(
    omnibox::RichAnswerTemplate* answer_template) {
  if (!(answer_template->answers_size() > 0)) {
    return;
  }
  std::string* url_string =
      answer_template->mutable_answers(0)->mutable_image()->mutable_url();
  answer_template->mutable_answers(0)->mutable_image()->set_url(
      omnibox::answer_data_parser::GetFormattedURL(url_string).spec());
}

}  // namespace

omnibox::SuggestSubtype SuggestSubtypeForNumber(int value) {
  // Note that ideally this should first check if `value` is valid by calling
  // omnibox::SuggestSubtype_IsValid and return omnibox::SUBTYPE_NONE when there
  // is no corresponding enum object. However, that is not possible because the
  // current list of subtypes in omnibox::SuggestSubtype is not exhaustive.
  // However, casting int values into omnibox::SuggestSubtype without testing
  // membership is expected to be safe as omnibox::SuggestSubtype has a fixed
  // int underlying type.
  return static_cast<omnibox::SuggestSubtype>(value);
}

omnibox::NavigationalIntent NavigationalIntentForNumber(int value) {
  if (omnibox::NavigationalIntent_IsValid(value)) {
    return static_cast<omnibox::NavigationalIntent>(value);
  }
  return omnibox::NavigationalIntent::NAV_INTENT_NONE;
}

omnibox::AnswerType AnswerTypeForNumber(int value) {
  if (omnibox::AnswerType_IsValid(value)) {
    return static_cast<omnibox::AnswerType>(value);
  }
  return omnibox::ANSWER_TYPE_UNSPECIFIED;
}

// SearchSuggestionParser::Result ----------------------------------------------

SearchSuggestionParser::Result::Result(
    bool from_keyword,
    int relevance,
    bool relevance_from_server,
    AutocompleteMatchType::Type type,
    omnibox::SuggestType suggest_type,
    std::vector<int> subtypes,
    const std::string& deletion_url,
    omnibox::NavigationalIntent navigational_intent)
    : from_keyword_(from_keyword),
      type_(type),
      suggest_type_(suggest_type),
      subtypes_(std::move(subtypes)),
      relevance_(relevance),
      relevance_from_server_(relevance_from_server),
      received_after_last_keystroke_(true),
      deletion_url_(deletion_url),
      navigational_intent_(navigational_intent) {}

SearchSuggestionParser::Result::Result(const Result& other) = default;

SearchSuggestionParser::Result::~Result() {}

// SearchSuggestionParser::SuggestResult ---------------------------------------

SearchSuggestionParser::SuggestResult::SuggestResult(
    const std::u16string& suggestion,
    AutocompleteMatchType::Type type,
    omnibox::SuggestType suggest_type,
    std::vector<int> subtypes,
    bool from_keyword,
    omnibox::NavigationalIntent navigational_intent,
    int relevance,
    bool relevance_from_server,
    const std::u16string& input_text)
    : SuggestResult(suggestion,
                    type,
                    suggest_type,
                    std::move(subtypes),
                    suggestion,
                    /*match_contents_prefix=*/std::u16string(),
                    /*annotation=*/std::u16string(),
                    /*entity_info=*/omnibox::EntityInfo(),
                    /*deletion_url=*/"",
                    from_keyword,
                    navigational_intent,
                    relevance,
                    relevance_from_server,
                    /*should_prefetch=*/false,
                    /*should_prerender=*/false,
                    input_text) {}

SearchSuggestionParser::SuggestResult::SuggestResult(
    const std::u16string& suggestion,
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
    const std::u16string& input_text)
    : Result(from_keyword,
             relevance,
             relevance_from_server,
             type,
             suggest_type,
             std::move(subtypes),
             deletion_url,
             navigational_intent),
      suggestion_(suggestion),
      match_contents_prefix_(match_contents_prefix),
      entity_info_(std::move(entity_info)),
      should_prefetch_(should_prefetch),
      should_prerender_(should_prerender) {
  annotation_ = !entity_info_.annotation().empty()
                    ? base::UTF8ToUTF16(entity_info_.annotation())
                    : annotation;
  match_contents_ = !entity_info_.name().empty()
                        ? base::UTF8ToUTF16(entity_info_.name())
                        : match_contents;
  match_contents_ = base::CollapseWhitespace(match_contents_, false);
  DCHECK(!match_contents_.empty());
  ClassifyMatchContents(true, input_text);
}

SearchSuggestionParser::SuggestResult::SuggestResult(
    const SuggestResult& result) = default;

SearchSuggestionParser::SuggestResult::~SuggestResult() {}

SearchSuggestionParser::SuggestResult&
SearchSuggestionParser::SuggestResult::operator=(const SuggestResult& rhs) =
    default;

void SearchSuggestionParser::SuggestResult::ClassifyMatchContents(
    const bool allow_bolding_all,
    const std::u16string& input_text) {
  DCHECK(!match_contents_.empty());

  // In case of zero-suggest results, do not highlight matches.
  if (input_text.empty()) {
    match_contents_class_ = {
        ACMatchClassification(0, ACMatchClassification::NONE)};
    return;
  }

  std::u16string lookup_text = input_text;
  if (type_ == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    const size_t contents_index =
        suggestion_.length() - match_contents_.length();
    // Ensure the query starts with the input text, and ends with the match
    // contents, and the input text has an overlap with contents.
    if (base::StartsWith(suggestion_, input_text,
                         base::CompareCase::SENSITIVE) &&
        base::EndsWith(suggestion_, match_contents_,
                       base::CompareCase::SENSITIVE) &&
        (input_text.length() > contents_index)) {
      lookup_text = input_text.substr(contents_index);
    }
  }
  // Do a case-insensitive search for |lookup_text|.
  std::u16string::const_iterator lookup_position = base::ranges::search(
      match_contents_, lookup_text, SimpleCaseInsensitiveCompareUCS2());
  if (!allow_bolding_all && (lookup_position == match_contents_.end())) {
    // Bail if the code below to update the bolding would bold the whole
    // string.  Note that the string may already be entirely bolded; if
    // so, leave it as is.
    return;
  }

  // Note we discard our existing match_contents_class_ with this call.
  match_contents_class_ =
      ClassifyAllMatchesInString(input_text, match_contents_, true);
}

void SearchSuggestionParser::SuggestResult::SetAnswer(
    const SuggestionAnswer& answer) {
  answer_ = answer;
}

void SearchSuggestionParser::SuggestResult::SetRichAnswerTemplate(
    const omnibox::RichAnswerTemplate& answer_template) {
  answer_template_ = answer_template;
}

void SearchSuggestionParser::SuggestResult::SetAnswerType(
    const omnibox::AnswerType& answer_type) {
  answer_type_ = answer_type;
}

void SearchSuggestionParser::SuggestResult::SetEntityInfo(
    const omnibox::EntityInfo& entity_info) {
  entity_info_ = entity_info;
}

int SearchSuggestionParser::SuggestResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  if (!from_keyword_ && keyword_provider_requested)
    return 100;
  return ((input.type() == metrics::OmniboxInputType::URL) ? 300 : 600);
}

// SearchSuggestionParser::NavigationResult ------------------------------------

SearchSuggestionParser::NavigationResult::NavigationResult(
    const AutocompleteSchemeClassifier& scheme_classifier,
    const GURL& url,
    AutocompleteMatchType::Type match_type,
    omnibox::SuggestType suggest_type,
    std::vector<int> subtypes,
    const std::u16string& description,
    const std::string& deletion_url,
    bool from_keyword,
    omnibox::NavigationalIntent navigational_intent,
    int relevance,
    bool relevance_from_server,
    const std::u16string& input_text)
    : Result(from_keyword,
             relevance,
             relevance_from_server,
             match_type,
             suggest_type,
             std::move(subtypes),
             deletion_url,
             navigational_intent),
      url_(url),
      formatted_url_(AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url,
          url_formatter::FormatUrl(url,
                                   url_formatter::kFormatUrlOmitDefaults &
                                       ~url_formatter::kFormatUrlOmitHTTP,
                                   base::UnescapeRule::SPACES,
                                   nullptr,
                                   nullptr,
                                   nullptr),
          scheme_classifier,
          nullptr)),
      description_(description) {
  DCHECK(url_.is_valid());
  CalculateAndClassifyMatchContents(true, input_text);
  ClassifyDescription(input_text);
}

SearchSuggestionParser::NavigationResult::NavigationResult(
    const NavigationResult& other) = default;

SearchSuggestionParser::NavigationResult::~NavigationResult() {}

void SearchSuggestionParser::NavigationResult::
    CalculateAndClassifyMatchContents(const bool allow_bolding_nothing,
                                      const std::u16string& input_text) {
  // Start with the trivial nothing-bolded classification.
  DCHECK(url_.is_valid());

  // In case of zero-suggest results, do not highlight matches.
  if (input_text.empty()) {
    // TODO(tommycli): Maybe this should actually return
    // ACMatchClassification::URL. I'm not changing this now because this CL
    // is meant to fix a regression only, but we should consider this for
    // consistency with other |input_text| that matches nothing.
    match_contents_class_ = {
        ACMatchClassification(0, ACMatchClassification::NONE)};
    return;
  }

  // Set contents to the formatted URL while ensuring the scheme and subdomain
  // are kept if the user text seems to include them. E.g., for the user text
  // 'http google.com', the contents should not trim 'http'.
  bool match_in_scheme = false;
  bool match_in_subdomain = false;
  TermMatches term_matches_in_url = FindTermMatches(input_text, formatted_url_);
  // Convert TermMatches (offset, length) to MatchPosition (start, end).
  std::vector<AutocompleteMatch::MatchPosition> match_positions;
  for (auto match : term_matches_in_url)
    match_positions.emplace_back(match.offset, match.offset + match.length);
  AutocompleteMatch::GetMatchComponents(GURL(formatted_url_), match_positions,
                                        &match_in_scheme, &match_in_subdomain);
  auto format_types = AutocompleteMatch::GetFormatTypes(
      GURL(input_text).has_scheme(), match_in_subdomain);

  // Find matches in the potentially new match_contents
  std::u16string match_contents =
      url_formatter::FormatUrl(url_, format_types, base::UnescapeRule::SPACES,
                               nullptr, nullptr, nullptr);
  TermMatches term_matches = FindTermMatches(input_text, match_contents);

  // Update |match_contents_| and |match_contents_class_| if it's allowed.
  if (allow_bolding_nothing || !term_matches.empty()) {
    match_contents_ = match_contents;
    match_contents_class_ = ClassifyTermMatches(
        term_matches, match_contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
  }
}

int SearchSuggestionParser::NavigationResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  return (from_keyword_ || !keyword_provider_requested) ? 800 : 150;
}

void SearchSuggestionParser::NavigationResult::ClassifyDescription(
    const std::u16string& input_text) {
  TermMatches term_matches = FindTermMatches(input_text, description_);
  description_class_ = ClassifyTermMatches(term_matches, description_.size(),
                                           ACMatchClassification::MATCH,
                                           ACMatchClassification::NONE);
}

// SearchSuggestionParser::Results ---------------------------------------------

SearchSuggestionParser::Results::Results()
    : verbatim_relevance(-1),
      field_trial_triggered(false),
      relevances_from_server(false) {}

SearchSuggestionParser::Results::~Results() {}

void SearchSuggestionParser::Results::Clear() {
  suggest_results.clear();
  navigation_results.clear();
  verbatim_relevance = -1;
  metadata.clear();
  field_trial_triggered = false;
  experiment_stats_v2s.clear();
  relevances_from_server = false;
  suggestion_groups_map.clear();
}

bool SearchSuggestionParser::Results::HasServerProvidedScores() const {
  if (verbatim_relevance >= 0)
    return true;

  // Right now either all results of one type will be server-scored or they will
  // all be locally scored, but in case we change this later, we'll just check
  // them all.
  for (auto i(suggest_results.begin()); i != suggest_results.end(); ++i) {
    if (i->relevance_from_server())
      return true;
  }
  for (auto i(navigation_results.begin()); i != navigation_results.end(); ++i) {
    if (i->relevance_from_server())
      return true;
  }

  return false;
}

// SearchSuggestionParser ------------------------------------------------------

// static
std::string SearchSuggestionParser::ExtractJsonData(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  const net::HttpResponseHeaders* response_headers = nullptr;
  if (source && source->ResponseInfo())
    response_headers = source->ResponseInfo()->headers.get();
  if (!response_body)
    return std::string();

  std::string json_data = std::move(*response_body);

  // JSON is supposed to be UTF-8, but some suggest service providers send
  // JSON files in non-UTF-8 encodings.  The actual encoding is usually
  // specified in the Content-Type header field.
  if (response_headers) {
    std::string charset;
    if (response_headers->GetCharset(&charset)) {
      std::u16string data_16;
      // TODO(jungshik): Switch to CodePageToUTF8 after it's added.
      if (base::CodepageToUTF16(json_data, charset.c_str(),
                                base::OnStringConversionError::FAIL, &data_16))
        json_data = base::UTF16ToUTF8(data_16);
    }
  }
  return json_data;
}

// static
std::optional<base::Value::List> SearchSuggestionParser::DeserializeJsonData(
    std::string_view json_data) {
  // The JSON response should be an array.
  for (size_t response_start_index = json_data.find("["), i = 0;
       response_start_index != std::string_view::npos && i < 5;
       response_start_index = json_data.find("[", 1), i++) {
    // Remove any XSSI guards to allow for JSON parsing.
    json_data.remove_prefix(response_start_index);

    std::optional<base::Value> data =
        base::JSONReader::Read(json_data, base::JSON_ALLOW_TRAILING_COMMAS);
    if (data && data->is_list()) {
      return std::move(data->GetList());
    }
  }
  return std::nullopt;
}

// static
bool SearchSuggestionParser::ParseSuggestResults(
    const base::Value::List& root_list,
    const AutocompleteInput& input,
    const AutocompleteSchemeClassifier& scheme_classifier,
    int default_result_relevance,
    bool is_keyword_result,
    Results* results) {
  const std::u16string input_text = input.IsZeroSuggest() ? u"" : input.text();

  // 1st element: query.
  if (root_list.empty() || !root_list[0].is_string())
    return false;
  std::u16string query = base::UTF8ToUTF16(root_list[0].GetString());
  if (query != input_text) {
    return false;
  }

  // 2nd element: suggestions list.
  if (root_list.size() < 2u || !root_list[1].is_list())
    return false;
  const auto& results_list = root_list[1].GetList();

  // 3rd element: Ignore the optional description list for now.
  // 4th element: Disregard the query URL list.
  // 5th element: Disregard the optional key-value pairs from the server.

  // Reset suggested relevance information.
  results->verbatim_relevance = -1;

  const base::Value::List* suggest_types = nullptr;
  const base::Value::List* suggest_subtypes = nullptr;
  const base::Value::List* nav_intents = nullptr;
  const base::Value::List* relevances = nullptr;
  const base::Value::List* suggestion_details = nullptr;
  const base::Value::List* subtype_identifiers = nullptr;
  int prefetch_index = -1;
  int prerender_index = -1;
  omnibox::GroupsInfo groups_info;

  if (root_list.size() > 4u && root_list[4].is_dict()) {
    const base::Value::Dict& extras = root_list[4].GetDict();

    suggest_types = extras.FindList("google:suggesttype");

    suggest_subtypes = extras.FindList("google:suggestsubtypes");

    nav_intents = extras.FindList("google:suggestnavintents");

    relevances = extras.FindList("google:suggestrelevance");
    // Discard this list if its size does not match that of the suggestions.
    if (relevances && relevances->size() != results_list.size()) {
      relevances = nullptr;
    }

    if (std::optional<int> relevance =
            extras.FindInt("google:verbatimrelevance")) {
      results->verbatim_relevance = *relevance;
    }

    // Check if the active suggest field trial (if any) has triggered either
    // for the default provider or keyword provider.
    std::optional<bool> field_trial_triggered =
        extras.FindBool("google:fieldtrialtriggered");
    results->field_trial_triggered = field_trial_triggered.value_or(false);

    results->experiment_stats_v2s.clear();
    const base::Value::List* experiment_stats_v2s_list =
        extras.FindList("google:experimentstats");
    if (experiment_stats_v2s_list) {
      for (const auto& experiment_stats_v2_value : *experiment_stats_v2s_list) {
        const base::Value::Dict* experiment_stats_v2_dict =
            experiment_stats_v2_value.GetIfDict();
        if (!experiment_stats_v2_dict) {
          continue;
        }
        std::optional<int> type_int =
            experiment_stats_v2_dict->FindInt(kTypeIntFieldNumber);
        const auto* string_value =
            experiment_stats_v2_dict->FindString(kStringValueFieldNumber);
        if (!type_int || !string_value) {
          continue;
        }
        omnibox::metrics::ChromeSearchboxStats::ExperimentStatsV2
            experiment_stats_v2;
        experiment_stats_v2.set_type_int(*type_int);
        experiment_stats_v2.set_string_value(*string_value);
        results->experiment_stats_v2s.push_back(std::move(experiment_stats_v2));
      }
    }

    const auto* groups_info_string = extras.FindString("google:groupsinfo");
    DecodeProtoFromBase64<omnibox::GroupsInfo>(groups_info_string, groups_info);

    const base::Value::Dict* client_data = extras.FindDict("google:clientdata");
    if (client_data) {
      prefetch_index = client_data->FindInt("phi").value_or(-1);
      prerender_index = client_data->FindInt("pre").value_or(-1);
    }

    suggestion_details = extras.FindList("google:suggestdetail");
    // Discard this list if its size does not match that of the suggestions.
    if (suggestion_details &&
        suggestion_details->size() != results_list.size()) {
      suggestion_details = nullptr;
    }

    // Legacy code: Get subtype identifiers.
    subtype_identifiers = extras.FindList("google:subtypeid");
    // Discard this list if its size does not match that of the suggestions.
    if (subtype_identifiers &&
        subtype_identifiers->size() != results_list.size()) {
      subtype_identifiers = nullptr;
    }

    // Store the metadata that came with the response in case we need to pass
    // it along with the prefetch query to Instant.
    base::JSONWriter::Write(extras, &results->metadata);
  }

  // Processed list of match subtypes, one vector per match.
  // Note: ParseMatchSubtypes will handle the cases where the key does not
  // exist or contains malformed data.
  std::vector<std::vector<int>> subtypes =
      ParseMatchSubtypes(suggest_subtypes, results_list.size());

  // Clear the previous results now that new results are available.
  results->suggest_results.clear();
  results->navigation_results.clear();

  std::string type;
  int relevance = default_result_relevance;
  const std::u16string& trimmed_input =
      base::CollapseWhitespace(input_text, false);

  for (size_t index = 0;
       index < results_list.size() && results_list[index].is_string();
       ++index) {
    std::u16string suggestion =
        base::UTF8ToUTF16(results_list[index].GetString());
    // Google search may return empty suggestions for weird input characters,
    // they make no sense at all and can cause problems in our code.
    suggestion = base::CollapseWhitespace(suggestion, false);
    if (suggestion.empty())
      continue;

    omnibox::NavigationalIntent nav_intent = omnibox::NAV_INTENT_NONE;
    if (nav_intents && index < nav_intents->size() &&
        (*nav_intents)[index].is_int()) {
      nav_intent = NavigationalIntentForNumber((*nav_intents)[index].GetInt());
    }

    // Apply valid suggested relevance scores; discard invalid lists.
    if (relevances) {
      if (!(*relevances)[index].is_int()) {
        relevances = nullptr;
      } else {
        relevance = (*relevances)[index].GetInt();
      }
    }

    AutocompleteMatchType::Type match_type =
        AutocompleteMatchType::SEARCH_SUGGEST;
    omnibox::SuggestType suggest_type = omnibox::TYPE_QUERY;

    // Legacy code: if the server sends us a single subtype ID, place it beside
    // other subtypes.
    if (subtype_identifiers && index < subtype_identifiers->size() &&
        (*subtype_identifiers)[index].is_int()) {
      subtypes[index].emplace_back((*subtype_identifiers)[index].GetInt());
    }

    if (suggest_types && index < suggest_types->size() &&
        (*suggest_types)[index].is_string()) {
      suggest_type = GetSuggestType((*suggest_types)[index].GetString());
      match_type = GetAutocompleteMatchType(suggest_type);
    }

    std::string deletion_url;
    if (suggestion_details && index < suggestion_details->size() &&
        (*suggestion_details)[index].is_dict()) {
      const base::Value::Dict& suggestion_detail =
          (*suggestion_details)[index].GetDict();
      deletion_url = FindStringOrEmpty(suggestion_detail, "du");
    }

    if ((match_type == AutocompleteMatchType::NAVSUGGEST) ||
        (match_type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED)) {
      // Do not blindly trust the URL coming from the server to be valid.
      GURL url(url_formatter::FixupURL(base::UTF16ToUTF8(suggestion),
                                       std::string()));
      if (url.is_valid()) {
        std::u16string title;
        // 3rd element: optional descriptions list
        if (root_list.size() > 2u && root_list[2].is_list()) {
          const auto& descriptions = root_list[2].GetList();
          if (index < descriptions.size() && descriptions[index].is_string()) {
            title = base::UTF8ToUTF16(descriptions[index].GetString());
          }
        }
        results->navigation_results.push_back(NavigationResult(
            scheme_classifier, url, match_type, suggest_type, subtypes[index],
            title, deletion_url, is_keyword_result, nav_intent, relevance,
            relevances != nullptr, input_text));
      }
    } else {
      std::u16string annotation;
      std::u16string match_contents = suggestion;
      if (match_type == AutocompleteMatchType::CALCULATOR) {
        const bool has_equals_prefix = !suggestion.compare(0, 2, u"= ");
        if (has_equals_prefix) {
          // Calculator results include a "= " prefix but we don't want to
          // include this in the search terms.
          suggestion.erase(0, 2);
          // Unlikely to happen, but better to be safe.
          if (base::CollapseWhitespace(suggestion, false).empty())
            continue;
        }
        if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP) {
          match_contents = l10n_util::GetStringFUTF16(
              IDS_OMNIBOX_ONE_LINE_CALCULATOR_SUGGESTION_TEMPLATE, query,
              suggestion);
        }
      }

      omnibox::RichSuggestTemplate suggest_template;
      omnibox::EntityInfo entity_info;
      std::u16string match_contents_prefix;
      std::optional<int> suggestion_group_id;
      bool answer_parsed_successfully = false;
      omnibox::RichAnswerTemplate answer_template;
      SuggestionAnswer answer;
      omnibox::AnswerType answer_type = omnibox::ANSWER_TYPE_UNSPECIFIED;

      if (suggestion_details && (*suggestion_details)[index].is_dict() &&
          !(*suggestion_details)[index].GetDict().empty()) {
        const base::Value::Dict& suggestion_detail =
            (*suggestion_details)[index].GetDict();

        // Rich Suggest Template.
        const auto* rich_template_str =
            suggestion_detail.FindString("google:templateinfo");
        DecodeProtoFromBase64<omnibox::RichSuggestTemplate>(rich_template_str,
                                                            suggest_template);

        // Entity.
        const auto* entity_info_string =
            suggestion_detail.FindString("google:entityinfo");
        DecodeProtoFromBase64<omnibox::EntityInfo>(entity_info_string,
                                                   entity_info);

        // Tail Suggest.
        std::string match_contents_tail =
            FindStringOrEmpty(suggestion_detail, "t");
        if (!match_contents_tail.empty()) {
          match_contents = base::UTF8ToUTF16(match_contents_tail);
        }
        match_contents_prefix =
            base::UTF8ToUTF16(FindStringOrEmpty(suggestion_detail, "mp"));

        // Suggestion group Id.
        suggestion_group_id = suggestion_detail.FindInt("zl");

        // Answer.
        const std::string* answer_type_str =
            suggestion_detail.FindString("ansb");
        if (answer_type_str) {
          // Check that answer type string can be mapped to omnibox::AnswerType.
          int numeric_answer_type = 0;
          if (base::StringToInt(base::UTF8ToUTF16(*answer_type_str),
                                &numeric_answer_type)) {
            base::UmaHistogramSparse("Omnibox.AnswerParseType",
                                     numeric_answer_type);
            answer_type = AnswerTypeForNumber(numeric_answer_type);
          }
        }
        if (answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED) {
          // omnibox::RichAnswerTemplate is preferred to "ansa" if available.
          if (suggest_template.has_rich_answer_template() &&
              !OmniboxFieldTrial::kAnswerActionsCounterfactual.Get()) {
            answer_template = suggest_template.rich_answer_template();
            FormatAnswerTemplateImageURL(&answer_template);
            // Ensure `answer_template` has an answer.
            answer_parsed_successfully = answer_template.answers_size() > 0;
          } else if (const auto* answer_json =
                         suggestion_detail.FindDict("ansa")) {
            if (omnibox_feature_configs::SuggestionAnswerMigration::Get()
                    .enabled) {
              answer_parsed_successfully =
                  omnibox::answer_data_parser::ParseJsonToAnswerData(
                      *answer_json, &answer_template);
            } else {
              answer_parsed_successfully = SuggestionAnswer::ParseAnswer(
                  *answer_json, answer_type, &answer);
            }
          }
          base::UmaHistogramBoolean("Omnibox.AnswerParseSuccess",
                                    answer_parsed_successfully);
        }
      }

      int int_index = static_cast<int>(index);
      bool should_prefetch = int_index == prefetch_index;
      bool should_prerender = int_index == prerender_index;
      results->suggest_results.push_back(
          SuggestResult(suggestion, match_type, suggest_type, subtypes[index],
                        match_contents, match_contents_prefix, annotation,
                        std::move(entity_info), deletion_url, is_keyword_result,
                        nav_intent, relevance, relevances != nullptr,
                        should_prefetch, should_prerender, trimmed_input));

      if (answer_parsed_successfully) {
        results->suggest_results.back().SetAnswerType(answer_type);
        if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled) {
          results->suggest_results.back().SetRichAnswerTemplate(
              answer_template);
        } else {
          results->suggest_results.back().SetAnswer(answer);
        }
      }

      if (suggestion_group_id) {
        results->suggest_results.back().set_suggestion_group_id(
            omnibox::GroupIdForNumber(*suggestion_group_id));
      }
    }
  }

  results->relevances_from_server = relevances != nullptr;

  // Keeps track of the position of the server-provided group IDs.
  size_t group_index = 0;

  // Adds the given group config to the results for the given group ID. Returns
  // true if the entry was added to or was already present in the results.
  auto add_group_config = [&](const omnibox::GroupId suggestion_group_id,
                              const omnibox::GroupConfig& group_config) {
    // Do not add the group config if the group ID is invalid or unknown to
    // Chrome.
    if (suggestion_group_id == omnibox::GROUP_INVALID) {
      return false;
    }

    // There is nothing to do if the group config has been added before.
    if (base::Contains(results->suggestion_groups_map, suggestion_group_id)) {
      return true;
    }

    // Store the group config with the appropriate section in the results.
    results->suggestion_groups_map[suggestion_group_id].MergeFrom(group_config);
    results->suggestion_groups_map[suggestion_group_id].set_section(
        ChromeGroupSectionForRemoteGroupIndex(group_index++));
    return true;
  };

  // Add the group configs associated with the suggestions.
  for (auto& suggest_result : results->suggest_results) {
    if (!suggest_result.suggestion_group_id().has_value()) {
      continue;
    }

    const omnibox::GroupId suggestion_group_id =
        suggest_result.suggestion_group_id().value();

    // Add the group config associated with the suggestion, if the suggestion
    // has a valid group ID and a corresponding group config is found in the
    // response.
    if (!base::Contains(groups_info.group_configs(), suggestion_group_id) ||
        !add_group_config(suggestion_group_id, groups_info.group_configs().at(
                                                   suggestion_group_id))) {
      continue;
    }
  }

  // Add the remaining group configs without any suggestions in the response.
  // The only known use case is the personalized zero-suggest which is also
  // produced by Chrome and relies on the server-provided group config to show
  // with the appropriate header text, where a header text is applicable.
  for (const auto& entry : groups_info.group_configs()) {
    add_group_config(omnibox::GroupIdForNumber(entry.first), entry.second);
  }

  return true;
}
