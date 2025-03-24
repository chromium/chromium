// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_identity_credential_delegate.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/federated_auth_autofill_source.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "ui/base/l10n/l10n_util.h"

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

  std::vector<Suggestion> suggestions;
  for (IdentityRequestAccountPtr account : *accounts) {
    Suggestion suggestion(base::UTF8ToUTF16(account->email),
                          SuggestionType::kIdentityCredential);

    suggestion.icon = Suggestion::Icon::kEmail;
    suggestion.minor_texts.emplace_back(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_IDENTITY_CREDENTIAL_MINOR_TEXT,
        base::UTF8ToUTF16(account->identity_provider->idp_for_display)));
    suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_IDENTITY_CREDENTIAL_EMAIL_LABEL))});
    suggestion.payload = Suggestion::IdentityCredentialPayload(
        account->identity_provider->idp_metadata.config_url, account->id);
    suggestions.push_back(std::move(suggestion));
  }

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

  source->NotifyAutofillSuggestionAccepted(payload.config_url,
                                           payload.account_id);
}

}  // namespace autofill
