// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

// Limit per enterprise suggestion type, not total matches.
size_t kMaxEnterpriseMatches = 40;

// Helper for reading possibly null paths from `base::Value::Dict`.
std::string ptr_to_string(const std::string* ptr) {
  return ptr ? *ptr : "";
}

// Helper for getting the correct TemplateURL based on the input.
const TemplateURL* AdjustTemplateURL(AutocompleteInput* input,
                                     TemplateURLService* turl_service) {
  DCHECK(turl_service);
  return input->InKeywordMode()
             ? AutocompleteInput::GetSubstitutingTemplateURLForInput(
                   turl_service, input)
             : turl_service->GetEnterpriseSearchAggregatorEngine();
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
  loader_ = std::move(loader);
}

void EnterpriseSearchAggregatorProvider::RequestCompleted(
    const network::SimpleURLLoader* source,
    int response_code,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

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
    // tracking the number of consecutive errors and only clearing matches if
    // the number of errors exceeds a certain threshold. Or verifying backoff
    // conditions from the server-side team.
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
  // Parse the results.
  const base::Value::List* queryResults = root_val.FindList("querySuggestions");
  const base::Value::List* peopleResults =
      root_val.FindList("peopleSuggestions");
  const base::Value::List* contentResults =
      root_val.FindList("contentSuggestions");

  ParseResultList(queryResults,
                  /*suggestion_type=*/SuggestionType::QUERY,
                  /*is_navigation=*/false);
  ParseResultList(peopleResults,
                  /*suggestion_type=*/SuggestionType::PEOPLE,
                  /*is_navigation=*/true);
  ParseResultList(contentResults,
                  /*suggestion_type=*/SuggestionType::CONTENT,
                  /*is_navigation=*/true);
}

void EnterpriseSearchAggregatorProvider::ParseResultList(
    const base::Value::List* results,
    SuggestionType suggestion_type,
    bool is_navigation) {
  if (!results) {
    return;
  }

  size_t num_results = results->size();
  // Limit the number items we add to `matches_`.
  if (num_results > kMaxEnterpriseMatches) {
    num_results = kMaxEnterpriseMatches;
  }

  for (size_t i = 0; i < num_results; i++) {
    const base::Value& result_value = (*results)[i];
    if (!result_value.is_dict()) {
      return;
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

    AutocompleteMatch match = CreateMatch(
        suggestion_type, is_navigation, 1000 - int(matches_.size()), url,
        image_url, icon_url, base::UTF8ToUTF16(description),
        base::UTF8ToUTF16(contents));
    matches_.push_back(match);
  }
}

std::string EnterpriseSearchAggregatorProvider::GetMatchDestinationUrl(
    const base::Value::Dict& result,
    const TemplateURLRef& url_ref,
    SuggestionType suggestion_type) const {
  if (suggestion_type == SuggestionType::CONTENT) {
    return ptr_to_string(
        result.FindStringByDottedPath("document.derivedStructData.link"));
  }

  std::string path = suggestion_type == SuggestionType::QUERY
                         ? "suggestion"
                         : "document.derivedStructData.name.userName";
  std::string query = ptr_to_string(result.FindStringByDottedPath(path));
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
  if (suggestion_type == SuggestionType::QUERY) {
    return ptr_to_string(result.FindString("suggestion"));
  } else if (suggestion_type == SuggestionType::PEOPLE) {
    return ptr_to_string(result.FindStringByDottedPath(
        "document.derivedStructData.name.userName"));
  }
  return "";
}

AutocompleteMatch EnterpriseSearchAggregatorProvider::CreateMatch(
    SuggestionType suggestion_type,
    bool is_navigation,
    int relevance,
    const std::string& url,
    const std::string& image_url,
    const std::string& icon_url,
    const std::u16string& description,
    const std::u16string& contents) {
  auto type = is_navigation ? AutocompleteMatchType::NAVSUGGEST
                            : AutocompleteMatchType::SEARCH_SUGGEST;
  AutocompleteMatch match(this, relevance, false, type);

  match.destination_url = GURL(url);
  match.fill_into_edit = base::UTF8ToUTF16(url);

  if (!image_url.empty()) {
    match.image_url = GURL(image_url);
  }

  if (!icon_url.empty()) {
    match.icon_url = GURL(icon_url);
  }

  match.enterprise_search_aggregator_type = suggestion_type;

  match.description = AutocompleteMatch::SanitizeString(description);
  match.description_class = ClassifyTermMatches(
      FindTermMatches(adjusted_input_.text(), match.description),
      match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);
  match.contents = AutocompleteMatch::SanitizeString(contents);
  match.contents_class = ClassifyTermMatches(
      FindTermMatches(adjusted_input_.text(), match.contents),
      match.contents.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);

  match.keyword = template_url_->keyword();
  match.transition = ui::PAGE_TRANSITION_KEYWORD;

  if (adjusted_input_.InKeywordMode()) {
    match.from_keyword = true;
  }

  match.RecordAdditionalInfo("aggregator type",
                             static_cast<int>(suggestion_type));

  return match;
}
