// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_identity_credential_delegate.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "content/public/browser/federated_auth_autofill_source.h"

namespace autofill {

ContentIdentityCredentialDelegate::ContentIdentityCredentialDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

std::vector<Suggestion>
ContentIdentityCredentialDelegate::GetVerifiedAutofillSuggestions(
    const AutofillField& field) const {
  std::optional<AutocompleteParsingResult> autocomplete =
      ParseAutocompleteAttribute(field.autocomplete_attribute());

  // Only <input autocomplete="email webidentity"> fields are considered.
  if (!autocomplete || !autocomplete->webidentity ||
      autocomplete->field_type != HtmlFieldType::kEmail ||
      ShouldIgnoreAutocompleteAttribute(field.autocomplete_attribute())) {
    return {};
  }

  // TODO(crbug.com/380367784): reproduce and add a test to make sure this
  // works properly when FedCM is called from inner frames.
  content::FederatedAuthAutofillSource* source =
      content::FederatedAuthAutofillSource::FromPage(
          web_contents_->GetPrimaryPage());

  if (!source) {
    return {};
  }

  std::optional<std::vector<IdentityRequestAccountPtr>> accounts =
      source->GetAutofillSuggestions();

  if (!accounts) {
    return {};
  }

  // TODO(crbug.com/380367784): transform the accounts suggestions into concrete
  // autofill suggestions.
  std::vector<Suggestion> suggestions;
  return suggestions;
}

void ContentIdentityCredentialDelegate::NotifySuggestionAccepted(
    const Suggestion& suggestion) const {
  content::FederatedAuthAutofillSource* source =
      content::FederatedAuthAutofillSource::FromPage(
          web_contents_->GetPrimaryPage());

  if (!source) {
    return;
  }

  Suggestion::IdentityCredentialPayload payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();

  // TODO(crbug.com/380367784): rename this to be more compatible with the
  // suggestion lifecycle, such as NotifySuggestionAccepted rather than
  // NotifyAutofillSelection (which can be understood as "hovering").
  source->NotifyAutofillSelection(payload.config_url, payload.account_id);
}

}  // namespace autofill
