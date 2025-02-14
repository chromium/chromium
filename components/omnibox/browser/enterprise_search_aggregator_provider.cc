// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
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
// If the user is in keyword mode, the input should not include the keyword.
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
    return;
  }

  // No need to redo or restart the previous request/response if the input
  // hasn't changed.
  if (minimal_changes)
    return;

  if (input.omit_asynchronous_matches()) {
    return;
  }

  // There should be no enterprise search suggestions fetched for on-focus
  // suggestion requests, or if the input is empty.
  if (input.IsZeroSuggest() ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    return;
  }

  // Request will always return 400 error response if the query is empty and
  // recent suggestions is not the only requested suggestion type. In keyword
  // mode, always clear matches_.
  // TODO(crbug.com/393480150): Remove this check once recent suggestions
  // are supported.
  if (input.InKeywordMode()) {
    auto adjusted_input = input;
    AdjustTemplateURL(&adjusted_input, template_url_service_);
    if (adjusted_input.text().empty()) {
      matches_.clear();
      return;
    }
  }

  input_ = input;
  done_ = false;  // Set true in callbacks.

  // Unretained is safe because `this` owns `debouncer_`.
  debouncer_->RequestRun(base::BindOnce(
      &EnterpriseSearchAggregatorProvider::Run, base::Unretained(this)));
}

void EnterpriseSearchAggregatorProvider::Stop(bool clear_cached_results,
                                              bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);
  debouncer_->CancelRequest();

  if (loader_) {
    loader_.reset();
  }
}

bool EnterpriseSearchAggregatorProvider::IsProviderAllowed(
    const AutocompleteInput& input) {
  // Don't start in incognito mode.
  if (client_->IsOffTheRecord()) {
    return false;
  }

  // There can be an aggregator set either through the feature params or through
  // a policy JSON. Both require this feature to be enabled.
  if (!omnibox_feature_configs::SearchAggregatorProvider::Get().enabled) {
    return false;
  }

  // TODO(crbug.com/380642693): Add backoff check.
  return true;
}

void EnterpriseSearchAggregatorProvider::Run() {
  auto adjusted_input = input_;
  const TemplateURL* template_url =
      AdjustTemplateURL(&adjusted_input, template_url_service_);

  CHECK(template_url);
  CHECK(template_url->policy_origin() ==
        TemplateURLData::PolicyOrigin::kSearchAggregator);

  // Don't clear `matches_` until a new successful response is ready to replace
  // them.

  client_->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
      ->CreateEnterpriseSearchAggregatorSuggestionsRequest(
          adjusted_input.text(), GURL(template_url->suggestions_url()),
          base::BindOnce(&EnterpriseSearchAggregatorProvider::RequestStarted,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &EnterpriseSearchAggregatorProvider::RequestCompleted,
              base::Unretained(this) /* this owns SimpleURLLoader */),
          input_.InKeywordMode());
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

  bool updated_matches = false;
  if (response_code == 200) {
    updated_matches = UpdateResults(SearchSuggestionParser::ExtractJsonData(
        source, std::move(response_body)));
  } else {
    // Clear matches for any response that is an error.
    // TODO(crbug.com/380642693): Add backoff if needed. This could be done by
    // tracking the number of consecutive errors and only clearing matches if
    // the number of errors exceeds a certain threshold. Or verifying backoff
    // conditions from the server-side team.
    matches_.clear();
    updated_matches = true;
  }

  loader_.reset();
  done_ = true;
  NotifyListeners(/*updated_matches=*/updated_matches);
}

bool EnterpriseSearchAggregatorProvider::UpdateResults(
    const std::string& json_data) {
  std::optional<base::Value::Dict> response =
      base::JSONReader::ReadDict(json_data, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!response) {
    return false;
  }

  // Clear old matches if received a successful response, even if the response
  // is empty.
  matches_.clear();

  // Fill `matches_` with the new server matches.
  ParseEnterpriseSearchAggregatorSearchResults(
      base::Value(std::move(*response)));

  return !matches_.empty();
}

void EnterpriseSearchAggregatorProvider::
    ParseEnterpriseSearchAggregatorSearchResults(const base::Value& root_val) {
  CHECK(root_val.is_dict());
  const TemplateURL* template_url =
      template_url_service_->GetEnterpriseSearchAggregatorEngine();

  // Parse the results.
  const base::Value::List* queryResults =
      root_val.GetDict().FindList("querySuggestions");
  const base::Value::List* peopleResults =
      root_val.GetDict().FindList("peopleSuggestions");
  const base::Value::List* contentResults =
      root_val.GetDict().FindList("contentSuggestions");

  ParseResultList(queryResults, template_url,
                  /*suggestion_type=*/SuggestionType::QUERY,
                  /*is_navigation=*/false);
  ParseResultList(peopleResults, template_url,
                  /*suggestion_type=*/SuggestionType::PEOPLE,
                  /*is_navigation=*/false);
  ParseResultList(contentResults, template_url,
                  /*suggestion_type=*/SuggestionType::CONTENT,
                  /*is_navigation=*/true);
}

void EnterpriseSearchAggregatorProvider::ParseResultList(
    const base::Value::List* results,
    const TemplateURL* template_url,
    SuggestionType suggestion_type,
    bool is_navigation) {
  if (!results || !template_url) {
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

    auto url = GetUrl(result, template_url->url_ref(), suggestion_type);
    // All matches must have a URL.
    if (url.empty()) {
      continue;
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
        input_, template_url->keyword(), suggestion_type, is_navigation,
        1000 - int(matches_.size()), url, base::UTF8ToUTF16(description),
        base::UTF8ToUTF16(contents));
    matches_.push_back(match);
  }
}

std::string EnterpriseSearchAggregatorProvider::GetUrl(
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
        "document.derivedStructData.name.userName"));
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
        "document.derivedStructData.name.displayName"));
  }
  return "";
}

AutocompleteMatch EnterpriseSearchAggregatorProvider::CreateMatch(
    const AutocompleteInput& input,
    const std::u16string& keyword,
    SuggestionType suggestion_type,
    bool is_navigation,
    int relevance,
    const std::string& url,
    const std::u16string& description,
    const std::u16string& contents) {
  auto type = is_navigation ? AutocompleteMatchType::NAVSUGGEST
                            : AutocompleteMatchType::SEARCH_SUGGEST;
  AutocompleteMatch match(this, relevance, false, type);

  match.destination_url = GURL(url);
  match.fill_into_edit = base::UTF8ToUTF16(url);

  match.description = AutocompleteMatch::SanitizeString(description);
  match.description_class = ClassifyTermMatches(
      FindTermMatches(input.text(), match.description),
      match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);
  match.contents = AutocompleteMatch::SanitizeString(contents);
  match.contents_class = ClassifyTermMatches(
      FindTermMatches(input.text(), match.contents), match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  match.keyword = keyword;
  match.transition = ui::PAGE_TRANSITION_KEYWORD;

  if (input.InKeywordMode()) {
    match.from_keyword = true;
  }

  match.RecordAdditionalInfo("aggregator type",
                             static_cast<int>(suggestion_type));

  return match;
}
