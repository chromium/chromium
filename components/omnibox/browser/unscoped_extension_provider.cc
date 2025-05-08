// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/unscoped_extension_provider.h"

#include <string>

#include "base/check_is_test.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/unscoped_extension_provider_delegate.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/search_engines/template_url_service.h"

UnscopedExtensionProvider::UnscopedExtensionProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION),
      client_(client),
      template_url_service_(client->GetTemplateURLService()),
      delegate_(client->GetUnscopedExtensionProviderDelegate(this)) {
  AddListener(listener);
}

UnscopedExtensionProvider::~UnscopedExtensionProvider() = default;

void UnscopedExtensionProvider::Start(const AutocompleteInput& input,
                                      bool minimal_changes) {
  // If the changes to the input are not minimal, clear the current list of
  // matches and suggestion group information and increment the current request
  // ID to discard any suggestions that may be incoming later with a stale
  // request ID.
  Stop(minimal_changes ? AutocompleteStopReason::kInteraction
                       : AutocompleteStopReason::kClobbered);

  // Unscoped mode input should not be redirected to an extension in incognito.
  if (client_->IsOffTheRecord()) {
    return;
  }

  // Extension suggestions are not allowed in keyword mode.
  if (input.InKeywordMode()) {
    return;
  }

  // See if zero suggest provider is eligible for zero suggest suggestions.
  // This prevents only unscoped extension suggestions from appearing when
  // other zps suggestions are not available.
  auto [_, eligible] =
      ZeroSuggestProvider::GetResultTypeAndEligibility(client_, input);

  if ((input.IsZeroSuggest() ||
       input.type() == metrics::OmniboxInputType::EMPTY) &&
      !eligible) {
    return;
  }

  // Extension suggestions are always provided asynchronously.
  if (input.omit_asynchronous_matches()) {
    return;
  }

  // Do not forward the input to the extensions delegate if the changes to the
  // input are minimal. If eligible for zero suggest (checked above), ignore
  // the minimal changes check.
  if (minimal_changes && !input.IsZeroSuggest()) {
    return;
  }

  // Do not forward the input to the extensions delegate if there are no
  // unscoped extensions.
  std::set<std::string> unscoped_extensions =
      GetTemplateURLService()->GetUnscopedModeExtensionIds();
  if (unscoped_extensions.empty()) {
    return;
  }

  // Forward the input to the extensions delegate.
  delegate_->Start(input, minimal_changes, unscoped_extensions);
}

void UnscopedExtensionProvider::Stop(AutocompleteStopReason stop_reason) {
  // Ignore the stop timer since extension suggestions might take longer than
  // 1500ms to generate (the stop timer gets triggered due to user inactivity).
  if (stop_reason == AutocompleteStopReason::kInactivity) {
    return;
  }
  AutocompleteProvider::Stop(stop_reason);
  delegate_->Stop(stop_reason == AutocompleteStopReason::kClobbered);
}

void UnscopedExtensionProvider::DeleteMatch(const AutocompleteMatch& match) {
  const std::u16string& suggestion_text = match.contents;
  std::erase_if(matches_, [&match](const AutocompleteMatch& i) {
    return i.keyword == match.keyword &&
           i.fill_into_edit == match.fill_into_edit;
  });

  const TemplateURL* const template_url =
      GetTemplateURLService()->GetTemplateURLForKeyword(match.keyword);

  if ((template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) &&
      delegate_) {
    delegate_->DeleteSuggestion(template_url, suggestion_text);
  }
}

TemplateURLService* UnscopedExtensionProvider::GetTemplateURLService() const {
  // Make sure the model is loaded. This is cheap and quickly bails out if
  // the model is already loaded.
  template_url_service_->Load();
  return template_url_service_;
}

void UnscopedExtensionProvider::AddToSuggestionGroupsMap(
    omnibox::GroupId group_id,
    omnibox::GroupConfig group_config) {
  suggestion_groups_map_[group_id].MergeFrom(group_config);
}
