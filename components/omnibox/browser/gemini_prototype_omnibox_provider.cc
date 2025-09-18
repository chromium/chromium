// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/gemini_prototype_omnibox_provider.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
// The relevance score for the Gemini suggestion. This is set to the highest
// value to ensure it appears as the first position.
const int kGeminiSuggestionRelevance = 1600;
}  // namespace

GeminiPrototypeOmniboxProvider::GeminiPrototypeOmniboxProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_GEMINI_PROTOTYPE),
      client_(client),
      service_(client->GetGeminiPrototypeOmniboxService()) {
  AddListener(listener);
}

GeminiPrototypeOmniboxProvider::~GeminiPrototypeOmniboxProvider() = default;

void GeminiPrototypeOmniboxProvider::Start(const AutocompleteInput& input,
                                           bool minimal_changes) {
  Stop(AutocompleteStopReason::kClobbered);

  // This provider should not run in synchronous contexts.
  if (input.omit_asynchronous_matches()) {
    return;
  }

  // This provider is gated by a feature flag.
  if (!omnibox::IsGeminiPrototypeProviderEnabled()) {
    return;
  }

  // If the user started typing, do not offer gemini matches.
  if (!input.IsZeroSuggest() && !input.text().empty()) {
    return;
  }

  // Provider requires a default search provider to create suggestions.
  const TemplateURL* default_provider =
      client_->GetTemplateURLService()->GetDefaultSearchProvider();
  if (!default_provider) {
    return;
  }

  matches_.clear();
  done_ = false;

  if (!service_) {
    return;
  }

  if (input.current_url() == last_url_ && !last_suggestion_.empty()) {
    OnSuggestionReceived(last_suggestion_);
    return;
  }

  last_url_ = input.current_url();
  service_->RequestSuggestions(
      input,
      base::BindOnce(&GeminiPrototypeOmniboxProvider::OnSuggestionReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GeminiPrototypeOmniboxProvider::Stop(AutocompleteStopReason stop_reason) {
  AutocompleteProvider::Stop(stop_reason);
  done_ = true;
  // Invalidate any pending callbacks to prevent use-after-free.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GeminiPrototypeOmniboxProvider::OnSuggestionReceived(
    const std::u16string& suggestion) {
  done_ = true;
  last_suggestion_ = suggestion;
  if (suggestion.empty()) {
    return;
  }

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    return;
  }

  // SEARCH_SUGGEST is used to get the classic "search" icon.
  AutocompleteMatch match(this, kGeminiSuggestionRelevance, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);

  // Set the group id in order to have the suggestion displayed in
  // IOSWebZpsSection.
  match.suggestion_group_id = omnibox::GROUP_VISITED_DOC_RELATED;

  match.contents = AutocompleteMatch::SanitizeString(suggestion);
  match.destination_url = GURL(default_provider->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(suggestion),
      client_->GetTemplateURLService()->search_terms_data()));
  match.contents_class.emplace_back(
      0, ACMatchClassification::MATCH | ACMatchClassification::URL);
  match.fill_into_edit = suggestion;
  match.transition = ui::PAGE_TRANSITION_GENERATED;
  match.keyword = default_provider->keyword();

  matches_.emplace_back(match);
  NotifyListeners(true);
}
