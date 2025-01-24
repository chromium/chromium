// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/search_engines/template_url_data.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

// static
EnterpriseSearchAggregatorProvider::EnterpriseSearchAggregatorProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(
          AutocompleteProvider::TYPE_ENTERPRISE_SEARCH_AGGREGATOR),
      client_(client),
      debouncer_(std::make_unique<AutocompleteProviderDebouncer>(true, 300)) {}

EnterpriseSearchAggregatorProvider::~EnterpriseSearchAggregatorProvider() =
    default;

bool EnterpriseSearchAggregatorProvider::IsProviderAllowed(
    const AutocompleteInput& input) {
  // Don't start in incognito mode.
  if (client_->IsOffTheRecord()) {
    return false;
  }

  // For now, only start if mock engines are valid.
  if (!omnibox_feature_configs::SearchAggregatorProvider::Get()
           .AreMockEnginesValid()) {
    return false;
  }

  // TODO(crbug.com/380642693): Add backoff check.
  return true;
}

void EnterpriseSearchAggregatorProvider::Start(const AutocompleteInput& input,
                                               bool minimal_changes) {
  Stop(/*clear_cached_results=*/!minimal_changes,
       /*due_to_user_inactivity=*/false);

  if (!IsProviderAllowed(input)) {
    return;
  }

  // No need to redo or restart the previous request/response if the input
  // hasn't changed.
  if (minimal_changes)
    return;

  input_ = input;
  done_ = false;  // Set true in callbacks.

  // Unretained is safe because `this` owns `debouncer_`.
  debouncer_->RequestRun(base::BindOnce(
      &EnterpriseSearchAggregatorProvider::Run, base::Unretained(this)));
}

void EnterpriseSearchAggregatorProvider::Run() {
  auto adjusted_input = input_;
  const TemplateURL* template_url =
      AutocompleteInput::GetSubstitutingTemplateURLForInput(
          client_->GetTemplateURLService(), &adjusted_input);
  CHECK(template_url);
  CHECK(template_url->featured_by_policy());
  CHECK(template_url->policy_origin() ==
        TemplateURLData::PolicyOrigin::kSearchAggregator);

  auto match = CreateMatch(input_, template_url->keyword(), true, 1500,
                           "https://wikipedia.org", u"Your document",
                           u"Last edited Feb 25");
  matches_.push_back(match);
  NotifyListeners(/*updated_matches=*/true);

  client_->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
      ->CreateEnterpriseSearchAggregatorSuggestionsRequest(
          adjusted_input.text(), GURL(template_url->suggestions_url()),
          base::BindOnce(&EnterpriseSearchAggregatorProvider::RequestStarted,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &EnterpriseSearchAggregatorProvider::RequestCompleted,
              base::Unretained(this) /* this owns SimpleURLLoader */));
}

void EnterpriseSearchAggregatorProvider::Stop(bool clear_cached_results,
                                              bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);
  debouncer_->CancelRequest();

  if (loader_) {
    loader_.reset();
  }
}

void EnterpriseSearchAggregatorProvider::RequestStarted(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  loader_ = std::move(loader);
}

void EnterpriseSearchAggregatorProvider::RequestCompleted(
    const network::SimpleURLLoader* source,
    const int response_code,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);
  done_ = true;
}

AutocompleteMatch EnterpriseSearchAggregatorProvider::CreateMatch(
    const AutocompleteInput& input,
    const std::u16string& keyword,
    bool is_navigation,
    int relevance,
    const std::string& url,
    const std::u16string& title,
    const std::u16string& additional_text) {
  auto type = is_navigation ? AutocompleteMatchType::NAVSUGGEST
                            : AutocompleteMatchType::SEARCH_SUGGEST;
  AutocompleteMatch match(this, relevance, false, type);

  match.destination_url = GURL(url);
  match.fill_into_edit = base::UTF8ToUTF16(url);

  match.description = title;
  match.description_class = ClassifyTermMatches(
      FindTermMatches(input.text(), match.description),
      match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);
  match.contents = additional_text;
  match.contents_class = ClassifyTermMatches(
      FindTermMatches(input.text(), match.contents), match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  match.keyword = keyword;
  match.transition = ui::PAGE_TRANSITION_KEYWORD;

  if (input.InKeywordMode())
    match.from_keyword = true;

  return match;
}
