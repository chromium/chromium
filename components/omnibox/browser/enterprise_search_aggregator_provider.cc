// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include <algorithm>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/string_cleaning.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

// Limit the number matches created for each type, not total, as a performance
// guard.
size_t kMaxMatchesCreatedPerType() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_max_matches_created_per_type;
}

// Limit the number of matches shown for each type, not total. Needed to prevent
// inputs like 'joe' or 'doc' from flooding the results with `PEOPLE` or
// `CONTENT` suggestions. More matches may be created in order to ensure the
// best matches are shown.
size_t kMaxScopedMatchesShownPerType() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_max_scoped_matches_shown_per_type;
}
size_t kMaxUnscopedMatchesShownPerType() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_max_unscoped_matches_shown_per_type;
}

// Score matches based on text similarity of the input and match fields.
// - Strong matches are input words at least 3 chars long that match the
//   suggestion content or description.
// - For PEOPLE suggestions, input words of 1 or 2 chars are strong matches if
//   they fully match (rather than prefix match) the suggestion content or
//   description. E.g. "jo" will be a strong match for "Jo Jacob", but "ja"
//   won't.
// - Weak matches are input words shorter than 3 chars or that match elsewhere
//   in the match fields.
// TODO(manukh): For consistency, rename "Text" to "Word" when finch params are
//   expired.
size_t kMinCharForStrongTextMatch() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_min_char_for_strong_text_match;
}

// If a) every input word is a strong match, and b) there are at least 2 such
// matches, score matches 1000.
size_t kMinWordsForFullTextMatchBoost() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_min_words_for_full_text_match_boost;
}
int kFullTextMatchScore() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_full_text_match_score;
}

// Otherwise, score using a weighted sum of the # of strong and weak matches.
int kScorePerStrongTextMatch() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_score_per_strong_text_match;
}
int kScorePerWeakTextMatch() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_score_per_weak_text_match;
}
int kMaxTextScore() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_max_text_score;
}

// Shift people relevances higher than calculated with the above constants. Most
// people-seeking inputs will have 2 words (firstname, lastname) and scoring
// these 800 wouldn't reliably allow them to make it to the final results.
int kPeopleScoreBoost() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_people_score_boost;
}

// When suggestions equally match the input, prefer showing content over query
// suggestions. This wont affect ranking due to grouping, only which suggestions
// are shown. This won't affect people suggestions unless `kPeopleScoreBoost` is
// 0.
bool kPreferContentsOverQueries() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_prefer_contents_over_queries;
}

// Always show at least 2 (unscoped) or 8 (scoped) suggestions if available.
// Only show more if they're scored at least 500; i.e. had at least 1 strong and
// 1 weak match.
size_t kScopedMaxLowQualityMatches() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_scoped_max_low_quality_matches;
}
size_t kUnscopedMaxLowQualityMatches() {
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_unscoped_max_low_quality_matches;
}
int kLowQualityThreshold() {
  // When this is converted back to a `constexpr`, it should be relative to
  // `scoring_score_per_strong_text_match` & `scoring_score_per_weak_text_match`
  // instead of an independent int.
  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .scoring_low_quality_threshold;
}

// Helper for reading possibly null paths from `base::Value::Dict`.
std::string ptr_to_string(const std::string* ptr) {
  return ptr ? *ptr : "";
}

struct MimeInfo {
  const std::string_view mime_type;
  const std::string_view file_type_description;
};

// A mapping from `mime_type` to the human readable `file_type_description`.
// Mappings documentation:
// https://developers.google.com/drive/api/guides/mime-types
// https://developers.google.com/drive/api/guides/ref-export-formats
// TODO(crbug.com/402436108): Localize the following strings.
const auto kMimeTypeMapping = base::MakeFixedFlatMap<std::string_view,
                                                     std::string_view>({
    {"application/vnd.google-apps.audio", "Audio"},
    {"application/vnd.google-apps.document", "Google Docs"},
    {"application/vnd.google-apps.drive-sdk", "Third-party shortcut"},
    {"application/vnd.google-apps.drawing", "Google Drawings"},
    {"application/vnd.google-apps.file", "Google Drive file"},
    {"application/vnd.google-apps.folder", "Google Drive folder"},
    {"application/vnd.google-apps.form", "Google Forms"},
    {"application/vnd.google-apps.fusiontable", "Google Fusion Tables"},
    {"application/vnd.google-apps.jam", "Google Jamboard"},
    {"application/vnd.google-apps.mail-layout", "Email layout"},
    {"application/vnd.google-apps.map", "Google My Maps"},
    {"application/vnd.google-apps.photo", "Google Photos"},
    {"application/vnd.google-apps.presentation", "Google Slides"},
    {"application/vnd.google-apps.script", "Google Apps Script"},
    {"application/vnd.google-apps.shortcut", "Shortcut"},
    {"application/vnd.google-apps.site", "Google Sites"},
    {"application/vnd.google-apps.spreadsheet", "Google Sheets"},
    {"application/vnd.google-apps.unknown", ""},
    {"application/vnd.google-apps.vid", "MP4"},
    {"application/vnd.google-apps.video", "Video"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document",
     "Microsoft Word"},
    {"application/vnd.oasis.opendocument.text", "OpenDocument"},
    {"application/rtf", "Rich Text"},
    {"application/pdf", "PDF"},
    {"text/plain", "Plain Text"},
    {"application/zip", "ZIP"},
    {"application/epub+zip", "EPUB ZIP"},
    {"text/markdown", "Markdown"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
     "Microsoft Excel"},
    {"application/x-vnd.oasis.opendocument.spreadsheet",
     "OpenDocument Spreadsheet"},
    {"text/csv", "Comma Separated Values"},
    {"text/tab-separated-values", "Tab Separated Values"},
    {"application/"
     "vnd.openxmlformats-officedocument.presentationml.presentation",
     "Microsoft PowerPoint"},
    {"application/vnd.oasis.opendocument.presentation", "ODP"},
    {"image/jpeg", "JPEG"},
    {"image/png", "PNG"},
    {"image/svg+xml", "Scalable Vector Graphics"},
    {"application/vnd.google-apps.script+json", "JSON"},
    {"video/quicktime", "Quicktime Video"},
});

// Helper for converting a `mime_type` into an abbreviated string.
std::string_view MimeToDescription(const std::string_view& mime_type) {
  const auto it = kMimeTypeMapping.find(mime_type);
  return it != kMimeTypeMapping.end() ? it->second : "";
}

// Helper for converting unix timestamp `time` into an abbreviated date.
// For time within the current day, return the time of day. (Ex. '12:45 PM')
// For time within the current year, return the abbreviated date. (Ex. 'Jan 02')
// Otherwise, return the full date. (Ex. '10/7/24')
const std::u16string UpdateTimeToString(std::optional<int> time) {
  if (!time) {
    return u"";
  }

  std::time_t unix_time = static_cast<std::time_t>(time.value());
  std::tm* local_time = std::localtime(&unix_time);
  if (!local_time) {
    return u"";
  }

  // Get current time to check if `unix_time` is in the current day or year.
  base::Time check_time = base::Time::FromTimeT(unix_time);
  base::Time now = base::Time::Now();

  return AutocompleteProvider::LocalizedLastModifiedString(now, check_time);
}

// Helper for getting the correct `TemplateURL` based on the input.
const TemplateURL* AdjustTemplateURL(AutocompleteInput* input,
                                     TemplateURLService* turl_service) {
  DCHECK(turl_service);
  return input->InKeywordMode()
             ? AutocompleteInput::GetSubstitutingTemplateURLForInput(
                   turl_service, input)
             : turl_service->GetEnterpriseSearchAggregatorEngine();
}

// Helpers to convert vector of strings to sets of words.
std::set<std::u16string> GetWords(std::vector<std::u16string> strings) {
  std::set<std::u16string> words = {};
  for (const auto& string : strings) {
    auto string_words = String16VectorFromString16(
        string_cleaning::CleanUpTitleForMatching(string), nullptr);
    std::move(string_words.begin(), string_words.end(),
              std::inserter(words, words.begin()));
  }
  return words;
}
std::set<std::u16string> GetWords(std::vector<std::string> strings) {
  std::vector<std::u16string> u16strings;
  std::ranges::transform(
      strings, std::back_inserter(u16strings),
      [](const auto& string) { return base::UTF8ToUTF16(string); });
  return GetWords(u16strings);
}

// Whether `word` matches any of `potential_match_words`.
enum class WordMatchType {
  NONE = 0,
  PREFIX,  // E.g. 'goo' prefixes 'goo' and 'google'.
  EXACT,   // E.g. 'goo' exactly matches 'goo' but not 'google'.
};
WordMatchType GetWordMatchType(std::u16string word,
                               std::set<std::u16string> potential_match_words) {
  auto it = potential_match_words.lower_bound(word);
  if (it == potential_match_words.end()) {
    return WordMatchType::NONE;
  }
  if (word == *it) {
    return WordMatchType::EXACT;
  }
  if (base::StartsWith(*it, word, base::CompareCase::SENSITIVE)) {
    return WordMatchType::PREFIX;
  }
  return WordMatchType::NONE;
}

// Returns 0 if the match should be filtered out.
EnterpriseSearchAggregatorProvider::RelevanceData CalculateRelevanceData(
    std::set<std::u16string> input_words,
    bool in_keyword_mode,
    AutocompleteMatch::EnterpriseSearchAggregatorType suggestion_type,
    const std::string& description,
    const std::string& contents,
    const std::vector<std::string> additional_scoring_fields) {
  // Split match fields into words.
  std::set<std::u16string> strong_scoring_words =
      GetWords({description, contents});
  std::set<std::u16string> weak_scoring_words =
      GetWords(additional_scoring_fields);

  // Compute text similarity of the input and match fields. See comment for
  // `kMinCharForStrongTextMatch`.
  size_t strong_word_matches = 0;
  size_t weak_word_matches = 0;
  for (const auto& input_word : input_words) {
    WordMatchType strong_match_type =
        GetWordMatchType(input_word, strong_scoring_words);
    if (strong_match_type == WordMatchType::EXACT &&
        suggestion_type ==
            AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE) {
      strong_word_matches++;
    } else if (strong_match_type != WordMatchType::NONE) {
      if (input_word.size() >= kMinCharForStrongTextMatch()) {
        strong_word_matches++;
      } else {
        weak_word_matches++;
      }
    } else if (GetWordMatchType(input_word, weak_scoring_words) !=
               WordMatchType::NONE) {
      weak_word_matches++;
    }
  }

  // Skip if there aren't at least 1 strong match or 2 weak matches.
  if (!in_keyword_mode && strong_word_matches == 0 && weak_word_matches < 2) {
    return {0, strong_word_matches, weak_word_matches,
            "less than 1 strong or 2 weak word matches"};
  }

  // Skip when less than half the input words had matches. The backend
  // prioritizes high recall, whereas most omnibox suggestions require every
  // input word to match.
  if ((strong_word_matches + weak_word_matches) * 2 < input_words.size()) {
    return {0, strong_word_matches, weak_word_matches,
            "less than half the input words matched"};
  }

  // Compute `relevance` using text similarity. See comments for
  // `kMinWordsForFullTextMatchBoost` & `kScorePerStrongTextMatch`.
  CHECK_LE(kMaxTextScore(), kFullTextMatchScore());
  int relevance = 0;
  if (strong_word_matches == input_words.size() &&
      strong_word_matches >= kMinWordsForFullTextMatchBoost()) {
    relevance = kFullTextMatchScore();
  } else {
    relevance = std::min(
        static_cast<int>(strong_word_matches) * kScorePerStrongTextMatch() +
            static_cast<int>(weak_word_matches) * kScorePerWeakTextMatch(),
        kMaxTextScore());
  }

  // People suggestions must match every input word. Otherwise, they feel bad;
  // e.g. 'omnibox c' shouldn't suggest 'Charles Aznavour'. This doesn't apply
  // to `QUERY` and `CONTENT` types because those might have fuzzy matches or
  // matches within their contents.
  if (suggestion_type ==
      AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE) {
    if (strong_word_matches + weak_word_matches < input_words.size()) {
      return {0, strong_word_matches, weak_word_matches,
              "unmatched input word for PEOPLE type"};
    } else {
      // See comment for `kPeopleScoreBoost`.
      relevance += kPeopleScoreBoost();
    }
  }

  // See comment for `kPreferContentsOverQueries`.
  if (suggestion_type ==
          AutocompleteMatch::EnterpriseSearchAggregatorType::CONTENT &&
      kPreferContentsOverQueries()) {
    // 10 is small enough to not cause showing a worse CONTENT match over a
    // better non-CONTENT match.
    relevance += 10;
  }

  return {relevance, strong_word_matches, weak_word_matches, "scored"};
}

void LogResultCounts(const base::Value::List* queryResults,
                     const base::Value::List* peopleResults,
                     const base::Value::List* contentResults) {
  size_t query_count = (queryResults ? queryResults->size() : 0);
  size_t people_count = (peopleResults ? peopleResults->size() : 0);
  size_t content_count = (contentResults ? contentResults->size() : 0);

  base::UmaHistogramExactLinear(
      "Omnibox.SuggestRequestsSent.ResultCount."
      "EnterpriseSearchAggregatorSuggest.Query",
      query_count, 50);

  base::UmaHistogramExactLinear(
      "Omnibox.SuggestRequestsSent.ResultCount."
      "EnterpriseSearchAggregatorSuggest.People",
      people_count, 50);

  base::UmaHistogramExactLinear(
      "Omnibox.SuggestRequestsSent.ResultCount."
      "EnterpriseSearchAggregatorSuggest.Content",
      content_count, 50);

  base::UmaHistogramExactLinear(
      "Omnibox.SuggestRequestsSent.ResultCount."
      "EnterpriseSearchAggregatorSuggest",
      query_count + people_count + content_count, 150);
}

}  // namespace

EnterpriseSearchAggregatorProvider::EnterpriseSearchAggregatorProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(
          AutocompleteProvider::TYPE_ENTERPRISE_SEARCH_AGGREGATOR),
      client_(client),
      debouncer_(std::make_unique<AutocompleteProviderDebouncer>(true, 300)),
      template_url_service_(client_->GetTemplateURLService()) {
  AddListener(listener);
}

EnterpriseSearchAggregatorProvider::~EnterpriseSearchAggregatorProvider() =
    default;

void EnterpriseSearchAggregatorProvider::Start(const AutocompleteInput& input,
                                               bool minimal_changes) {
  // Don't clear matches. Keep showing old matches until a new response comes.
  // This avoids flickering.
  Stop(/*clear_cached_results=*/false,
       /*due_to_user_inactivity=*/false);

  if (!IsProviderAllowed(input)) {
    // Clear old matches if provider is not allowed.
    matches_.clear();
    return;
  }

  // No need to redo or restart the previous request/response if the input
  // hasn't changed.
  if (minimal_changes) {
    return;
  }

  if (input.omit_asynchronous_matches()) {
    return;
  }

  adjusted_input_ = input;
  template_url_ = AdjustTemplateURL(&adjusted_input_, template_url_service_);
  CHECK(template_url_);
  CHECK(template_url_->policy_origin() ==
        TemplateURLData::PolicyOrigin::kSearchAggregator);

  // There should be no enterprise search suggestions fetched for on-focus
  // suggestion requests, or if the input is empty. Don't check
  // `OmniboxInputType::EMPTY` as the input's type isn't updated when keyword
  // adjusting.
  // TODO(crbug.com/393480150): Update this check once recent suggestions are
  //   supported.
  if (adjusted_input_.IsZeroSuggest() || adjusted_input_.text().empty()) {
    matches_.clear();
    return;
  }

  done_ = false;  // Set true in callbacks.

  // Unretained is safe because `this` owns `debouncer_`.
  debouncer_->RequestRun(base::BindOnce(
      &EnterpriseSearchAggregatorProvider::Run, base::Unretained(this)));
}

void EnterpriseSearchAggregatorProvider::Stop(bool clear_cached_results,
                                              bool due_to_user_inactivity) {
  // Ignore the stop timer since this provider is expected to take longer than
  // 1500ms (the stop timer gets triggered due to user inactivity).
  if (!due_to_user_inactivity) {
    AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);
    debouncer_->CancelRequest();
    if (loader_) {
      LogResponseTime(true);
      loader_.reset();
    }
  }
}

bool EnterpriseSearchAggregatorProvider::IsProviderAllowed(
    const AutocompleteInput& input) {
  // Don't start in incognito mode.
  if (client_->IsOffTheRecord()) {
    return false;
  }

  // Gate on "Improve Search Suggestions" setting.
  if (!client_->SearchSuggestEnabled()) {
    return false;
  }

  // There can be an aggregator set either through the feature params or through
  // a policy JSON. Both require this feature to be enabled.
  if (!omnibox_feature_configs::SearchAggregatorProvider::Get().enabled) {
    return false;
  }

  // Google must be set as default search provider.
  if (!search::DefaultSearchProviderIsGoogle(
          client_->GetTemplateURLService())) {
    return false;
  }

  // Don't run provider in non-keyword mode if query length is less than
  // the minimum length.
  if (!input.InKeywordMode() &&
      static_cast<int>(input.text().length()) <
          omnibox_feature_configs::SearchAggregatorProvider::Get()
              .min_query_length) {
    return false;
  }

  // Don't run provider if the input is a URL.
  if (input.type() == metrics::OmniboxInputType::URL) {
    return false;
  }

  // TODO(crbug.com/380642693): Add backoff check.
  return true;
}

void EnterpriseSearchAggregatorProvider::Run() {
  // Don't clear `matches_` until a new successful response is ready to replace
  // them.
  client_->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
      ->CreateEnterpriseSearchAggregatorSuggestionsRequest(
          adjusted_input_.text(), GURL(template_url_->suggestions_url()),
          base::BindOnce(&EnterpriseSearchAggregatorProvider::RequestStarted,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &EnterpriseSearchAggregatorProvider::RequestCompleted,
              base::Unretained(this) /* this owns SimpleURLLoader */),
          adjusted_input_.InKeywordMode());
}

void EnterpriseSearchAggregatorProvider::RequestStarted(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  SetTimeRequestSent();
  loader_ = std::move(loader);
}

void EnterpriseSearchAggregatorProvider::RequestCompleted(
    const network::SimpleURLLoader* source,
    int response_code,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);
  LogResponseTime(false);
  if (response_code == 200) {
    // Parse `response_body` in utility process if feature param is true.
    const std::string& json_data = SearchSuggestionParser::ExtractJsonData(
        source, std::move(response_body));
    if (omnibox_feature_configs::SearchAggregatorProvider::Get()
            .parse_response_in_utility_process) {
      data_decoder::DataDecoder::ParseJsonIsolated(
          json_data,
          base::BindOnce(
              &EnterpriseSearchAggregatorProvider::OnJsonParsedIsolated,
              base::Unretained(this)));
    } else {
      std::optional<base::Value::Dict> value = base::JSONReader::ReadDict(
          json_data, base::JSON_ALLOW_TRAILING_COMMAS);
      UpdateResults(value, response_code);
    }
  } else {
    // TODO(crbug.com/380642693): Add backoff if needed. This could be done by
    //   tracking the number of consecutive errors and only clearing matches if
    //   the number of errors exceeds a certain threshold. Or verifying backoff
    //   conditions from the server-side team.
    UpdateResults(std::nullopt, response_code);
  }
}

void EnterpriseSearchAggregatorProvider::OnJsonParsedIsolated(
    base::expected<base::Value, std::string> result) {
  std::optional<base::Value::Dict> value = std::nullopt;
  if (result.has_value()) {
    if (result.value().is_dict()) {
      value = std::move(result.value().GetDict());
    }
  }
  UpdateResults(value, 200);
}

void EnterpriseSearchAggregatorProvider::UpdateResults(
    const std::optional<base::Value::Dict>& response_value,
    int response_code) {
  bool updated_matches = false;

  if (response_value.has_value()) {
    // Clear old matches if received a successful response, even if the response
    // is empty.
    matches_.clear();
    ParseEnterpriseSearchAggregatorSearchResults(response_value.value());
    updated_matches = true;
  } else if (response_code != 200) {
    // Clear matches for any response that is an error.
    matches_.clear();
    updated_matches = true;
  }

  loader_.reset();
  done_ = true;
  NotifyListeners(/*updated_matches=*/updated_matches);
}

void EnterpriseSearchAggregatorProvider::
    ParseEnterpriseSearchAggregatorSearchResults(
        const base::Value::Dict& root_val) {
  // Break the input into words to avoid redoing this for every match.
  std::set<std::u16string> input_words = GetWords({adjusted_input_.text()});

  // Parse the results.
  const base::Value::List* queryResults = root_val.FindList("querySuggestions");
  const base::Value::List* peopleResults =
      root_val.FindList("peopleSuggestions");
  const base::Value::List* contentResults =
      root_val.FindList("contentSuggestions");

  ParseResultList(input_words, queryResults,
                  /*suggestion_type=*/SuggestionType::QUERY,
                  /*is_navigation=*/false);
  ParseResultList(input_words, peopleResults,
                  /*suggestion_type=*/SuggestionType::PEOPLE,
                  /*is_navigation=*/true);
  ParseResultList(input_words, contentResults,
                  /*suggestion_type=*/SuggestionType::CONTENT,
                  /*is_navigation=*/true);

  LogResultCounts(queryResults, peopleResults, contentResults);

  // Limit low-quality suggestions. See comment for
  // `kScopedMaxLowQualityMatches`.
  std::ranges::sort(matches_, std::ranges::greater{},
                    &AutocompleteMatch::relevance);
  size_t matches_to_keep = adjusted_input_.InKeywordMode()
                               ? kScopedMaxLowQualityMatches()
                               : kUnscopedMaxLowQualityMatches();
  if (matches_.size() > matches_to_keep) {
    for (; matches_to_keep < matches_.size(); ++matches_to_keep) {
      if (matches_[matches_to_keep].relevance < kLowQualityThreshold()) {
        break;
      }
    }
    matches_.erase(matches_.begin() + matches_to_keep, matches_.end());
  }
}

void EnterpriseSearchAggregatorProvider::ParseResultList(
    std::set<std::u16string> input_words,
    const base::Value::List* results,
    SuggestionType suggestion_type,
    bool is_navigation) {
  if (!results) {
    return;
  }

  // Limit # of matches created. See comment for `kMaxMatchesCreatedPerType`.
  size_t num_results = std::min(results->size(), kMaxMatchesCreatedPerType());

  ACMatches matches;
  for (size_t i = 0; i < num_results; i++) {
    const base::Value& result_value = (*results)[i];
    if (!result_value.is_dict()) {
      continue;
    }

    const base::Value::Dict& result = result_value.GetDict();

    auto url = GetMatchDestinationUrl(result, template_url_->url_ref(),
                                      suggestion_type);
    // All matches must have a URL.
    if (url.empty()) {
      continue;
    }

    // Some matches are supplied with an associated icon or image URL.
    std::string image_url;
    std::string icon_url;
    if (suggestion_type == SuggestionType::PEOPLE) {
      image_url = ptr_to_string(result.FindStringByDottedPath(
          "document.derivedStructData.displayPhoto.url"));
      // Ensure that image URLs from lh3.googleusercontent.com include an image
      // size parameter.
      if (base::StartsWith(image_url, "https://lh3.googleusercontent.com")) {
        // Check for existing size parameters (e.g., -s128, =w256, -h64).
        RE2 size_regex("[-=][s|w|h]\\d+");
        if (!RE2::PartialMatch(image_url, size_regex)) {
          image_url += base::Contains(image_url, "=") ? "-s64" : "=s64";
        }
      }
    } else if (suggestion_type == SuggestionType::CONTENT) {
      icon_url = ptr_to_string(result.FindStringByDottedPath("iconUri"));
    }

    auto description = GetMatchDescription(result, suggestion_type);
    // Nav matches must have a description.
    if (is_navigation && description.empty()) {
      continue;
    }

    auto contents = GetMatchContents(result, suggestion_type);
    // Search matches must have contents.
    if (!is_navigation && contents.empty()) {
      continue;
    }

    auto additional_scoring_fields =
        GetAdditionalScoringFields(result, suggestion_type);
    auto relevance_data = CalculateRelevanceData(
        input_words, adjusted_input_.InKeywordMode(), suggestion_type,
        description, contents, additional_scoring_fields);
    if (relevance_data.relevance) {
      // Decrement scores to keep sorting stable. Add 10 to avoid going below
      // "weak" threshold or change the hundred's digit; e.g. a score of
      // 600 v 599 could drastically affect the match's omnibox ranking.
      relevance_data.relevance += 10 - matches.size();
    }

    std::u16string fill_into_edit;
    if (adjusted_input_.InKeywordMode()) {
      fill_into_edit.append(template_url_->keyword() + u' ');
    }
    fill_into_edit.append(base::UTF8ToUTF16(is_navigation ? url : contents));

    matches.push_back(CreateMatch(suggestion_type, is_navigation,
                                  relevance_data, url, image_url, icon_url,
                                  base::UTF8ToUTF16(description),
                                  base::UTF8ToUTF16(contents), fill_into_edit));
  }

  // Limit # of matches added. See comment for
  // `kMaxScopedMatchesShownPerType`.
  size_t matches_to_add = adjusted_input_.InKeywordMode()
                              ? kMaxScopedMatchesShownPerType()
                              : kMaxUnscopedMatchesShownPerType();
  if (matches_to_add < matches.size()) {
    std::ranges::partial_sort(matches, matches.begin() + matches_to_add,
                              std::ranges::greater{},
                              &AutocompleteMatch::relevance);
    matches.erase(matches.begin() + matches_to_add, matches.end());
  }

  std::ranges::move(matches, std::back_inserter(matches_));
}

std::string EnterpriseSearchAggregatorProvider::GetMatchDestinationUrl(
    const base::Value::Dict& result,
    const TemplateURLRef& url_ref,
    SuggestionType suggestion_type) const {
  if (suggestion_type == SuggestionType::CONTENT) {
    std::string destination_uri =
        ptr_to_string(result.FindString("destinationUri"));
    // TODO(crbug.com/403545926): Remove support for
    //   "document.derivedStructData.link" once the change to populate
    //   "destinationUri" is available in prod.
    if (destination_uri.empty()) {
      destination_uri = ptr_to_string(
          result.FindStringByDottedPath("document.derivedStructData.link"));
    }
    return destination_uri;
  }

  std::string query = ptr_to_string(result.FindString("suggestion"));
  if (query.empty()) {
    return "";
  }

  return url_ref.ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(base::UTF8ToUTF16(query)), {}, nullptr);
}

std::string EnterpriseSearchAggregatorProvider::GetMatchDescription(
    const base::Value::Dict& result,
    SuggestionType suggestion_type) const {
  if (suggestion_type == SuggestionType::PEOPLE) {
    return ptr_to_string(result.FindStringByDottedPath(
        "document.derivedStructData.name.displayName"));
  } else if (suggestion_type == SuggestionType::CONTENT) {
    return ptr_to_string(
        result.FindStringByDottedPath("document.derivedStructData.title"));
  }
  return "";
}

std::string EnterpriseSearchAggregatorProvider::GetMatchContents(
    const base::Value::Dict& result,
    SuggestionType suggestion_type) const {
  if (suggestion_type == SuggestionType::QUERY ||
      suggestion_type == SuggestionType::PEOPLE) {
    return ptr_to_string(result.FindString("suggestion"));
  } else if (suggestion_type == SuggestionType::CONTENT) {
    std::optional<int> response_time =
        result.FindIntByDottedPath("document.derivedStructData.updated_time");
    const std::u16string last_updated = UpdateTimeToString(response_time);
    const std::u16string owner = base::UTF8ToUTF16(ptr_to_string(
        result.FindStringByDottedPath("document.derivedStructData.owner")));
    const std::u16string file_type_description = base::UTF8ToUTF16(
        MimeToDescription(ptr_to_string(result.FindStringByDottedPath(
            "document.derivedStructData.mime_type"))));
    return base::UTF16ToUTF8(GetLocalizedContentMetadata(
        last_updated, owner, file_type_description));
  }

  return "";
}

std::u16string EnterpriseSearchAggregatorProvider::GetLocalizedContentMetadata(
    const std::u16string& update_time,
    const std::u16string& owner,
    const std::u16string& file_type_description) const {
  if (!update_time.empty()) {
    if (!owner.empty()) {
      return !file_type_description.empty()
                 ? l10n_util::GetStringFUTF16(
                       IDS_CONTENT_SUGGESTION_DESCRIPTION_TEMPLATE, update_time,
                       owner, file_type_description)
                 : l10n_util::GetStringFUTF16(
                       IDS_CONTENT_SUGGESTION_DESCRIPTION_TEMPLATE_WITHOUT_FILE_TYPE_DESCRIPTION,
                       update_time, owner);
    }
    return !file_type_description.empty()
               ? l10n_util::GetStringFUTF16(
                     IDS_CONTENT_SUGGESTION_DESCRIPTION_TEMPLATE_WITHOUT_OWNER,
                     update_time, file_type_description)
               : update_time;
  }
  if (!owner.empty()) {
    return !file_type_description.empty()
               ? l10n_util::GetStringFUTF16(
                     IDS_CONTENT_SUGGESTION_DESCRIPTION_TEMPLATE_WITHOUT_DATE,
                     owner, file_type_description)
               : owner;
  }
  return !file_type_description.empty() ? file_type_description : u"";
}

std::vector<std::string>
EnterpriseSearchAggregatorProvider::GetAdditionalScoringFields(
    const base::Value::Dict& result,
    SuggestionType suggestion_type) const {
  // Should not return any fields already included in `GetMatchDescription()` &
  // `GetMatchContents()`.
  if (suggestion_type == SuggestionType::PEOPLE) {
    return {
        ptr_to_string(result.FindString("suggestion")),
        ptr_to_string(result.FindStringByDottedPath(
            "document.derivedStructData.name.givenName")),
        ptr_to_string(result.FindStringByDottedPath(
            "document.derivedStructData.name.familyName")),
        ptr_to_string(result.FindStringByDottedPath(
            "document.derivedStructData.emails.value")),
    };
  } else if (suggestion_type == SuggestionType::CONTENT) {
    return {
        ptr_to_string(
            result.FindStringByDottedPath("document.derivedStructData.owner")),
        ptr_to_string(result.FindStringByDottedPath(
            "document.derivedStructData.mime_type")),
        ptr_to_string(result.FindStringByDottedPath(
            "document.derivedStructData.owner_email")),
    };
  }
  return {};
}

AutocompleteMatch EnterpriseSearchAggregatorProvider::CreateMatch(
    SuggestionType suggestion_type,
    bool is_navigation,
    RelevanceData relevance_data,
    const std::string& url,
    const std::string& image_url,
    const std::string& icon_url,
    const std::u16string& description,
    const std::u16string& contents,
    const std::u16string& fill_into_edit) {
  auto type = is_navigation ? AutocompleteMatchType::NAVSUGGEST
                            : AutocompleteMatchType::SEARCH_SUGGEST;
  AutocompleteMatch match(this, relevance_data.relevance, false, type);

  match.destination_url = GURL(url);

  if (!image_url.empty()) {
    match.image_url = GURL(image_url);
  }

  if (!icon_url.empty()) {
    match.icon_url = GURL(icon_url);
  }

  match.enterprise_search_aggregator_type = suggestion_type;
  match.description = AutocompleteMatch::SanitizeString(description);
  match.contents = AutocompleteMatch::SanitizeString(contents);
  if (!is_navigation) {
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(match.contents);
  }

  // `NAVSUGGEST` is displayed "<description> - <contents>" and
  // `SEARCH_SUGGEST` is displayed "<contents> - <description>".
  // The below code formats `description` and `contents` accordingly.
  auto primary_text_class = [&](auto text) {
    return ClassifyTermMatches(FindTermMatches(adjusted_input_.text(), text),
                               text.size(), ACMatchClassification::MATCH,
                               ACMatchClassification::NONE);
  };
  ACMatchClassifications secondary_text_class =
      (contents.empty() || description.empty())
          ? std::vector<ACMatchClassification>{}
          : std::vector<ACMatchClassification>{{0, ACMatchClassification::DIM}};
  match.description_class = is_navigation
                                ? primary_text_class(match.description)
                                : secondary_text_class;
  match.contents_class =
      is_navigation ? secondary_text_class : primary_text_class(match.contents);
  match.fill_into_edit = fill_into_edit;

  match.keyword = template_url_->keyword();
  match.transition = adjusted_input_.InKeywordMode()
                         ? ui::PAGE_TRANSITION_KEYWORD
                         : ui::PAGE_TRANSITION_GENERATED;

  if (adjusted_input_.InKeywordMode()) {
    match.from_keyword = true;
  }

  match.RecordAdditionalInfo("aggregator type",
                             static_cast<int>(suggestion_type));
  match.RecordAdditionalInfo(
      "relevance strong word matches",
      static_cast<int>(relevance_data.strong_word_matches));
  match.RecordAdditionalInfo(
      "relevance weak word matches",
      static_cast<int>(relevance_data.weak_word_matches));
  match.RecordAdditionalInfo("relevance rule", relevance_data.rule);

  return match;
}

void EnterpriseSearchAggregatorProvider::SetTimeRequestSent() {
  client_->GetRemoteSuggestionsService(/*create_if_necessary=*/false)
      ->SetTimeRequestSent(
          RemoteRequestType::kEnterpriseSearchAggregatorSuggest,
          base::TimeTicks::Now());
}

void EnterpriseSearchAggregatorProvider::LogResponseTime(bool interrupted) {
  client_->GetRemoteSuggestionsService(/*create_if_necessary=*/false)
      ->LogResponseTime(RemoteRequestType::kEnterpriseSearchAggregatorSuggest,
                        interrupted);
}
