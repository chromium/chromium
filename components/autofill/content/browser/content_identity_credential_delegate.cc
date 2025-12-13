// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_identity_credential_delegate.h"

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/browser/suggestions/identity_credential_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/identity_credential/identity_credential.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/webid/autofill_source.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

ContentIdentityCredentialDelegate::ContentIdentityCredentialDelegate(
    content::WebContents* web_contents)
    : ContentIdentityCredentialDelegate(base::BindRepeating(
          [](content::WebContents* web_contents)
              -> content::webid::AutofillSource* {
            return content::webid::AutofillSource::FromPage(
                web_contents->GetPrimaryPage());
          },
          web_contents)) {}

ContentIdentityCredentialDelegate::ContentIdentityCredentialDelegate(
    base::RepeatingCallback<content::webid::AutofillSource*()> source)
    : source_(std::move(source)) {}

ContentIdentityCredentialDelegate::~ContentIdentityCredentialDelegate() =
    default;

std::vector<Suggestion>
ContentIdentityCredentialDelegate::GetVerifiedAutofillSuggestions(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const AutofillClient& client) const {
  std::vector<Suggestion> suggestions;
  IdentityCredentialSuggestionGenerator
      identity_credential_suggestion_generator(source_);

  auto on_suggestions_generated =
      [&suggestions](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions = std::move(returned_suggestions.second);
      };

  auto on_suggestion_data_returned =
      [&on_suggestions_generated, &form, &field, &form_structure,
       &autofill_field, &client, &identity_credential_suggestion_generator](
          std::pair<SuggestionGenerator::SuggestionDataSource,
                    std::vector<SuggestionGenerator::SuggestionData>>
              suggestion_data) {
        identity_credential_suggestion_generator.GenerateSuggestions(
            form, field, form_structure, autofill_field, client,
            {std::move(suggestion_data)}, on_suggestions_generated);
      };

  // Since the `on_suggestions_generated` callback is called synchronously,
  // we can assume that `suggestions` will hold correct value.
  identity_credential_suggestion_generator.FetchSuggestionData(
      form, field, form_structure, autofill_field, client,
      on_suggestion_data_returned);
  return suggestions;
}

void ContentIdentityCredentialDelegate::NotifySuggestionAccepted(
    const Suggestion& suggestion,
    bool show_modal,
    OnFederatedTokenReceivedCallback callback) const {
  content::webid::AutofillSource* source = source_.Run();

  if (!source) {
    return;
  }

  Suggestion::IdentityCredentialPayload payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();

  source->NotifyAutofillSuggestionAccepted(
      payload.config_url, payload.account_id, show_modal, std::move(callback));
}

std::unique_ptr<SuggestionGenerator>
ContentIdentityCredentialDelegate::GetIdentityCredentialSuggestionGenerator() {
  return std::make_unique<IdentityCredentialSuggestionGenerator>(source_);
}

}  // namespace autofill
