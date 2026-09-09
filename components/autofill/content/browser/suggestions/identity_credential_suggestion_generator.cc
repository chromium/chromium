// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/suggestions/identity_credential_suggestion_generator.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/identity_credential/identity_credential.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/webid/autofill_source.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

Suggestion CreateIdentityCredentialSuggestion(
    const IdentityCredential& credential,
    FieldType field_type) {
  CHECK_EQ(field_type, PASSWORD);
  Suggestion suggestion(SuggestionType::kIdentityCredential);
  suggestion.payload = Suggestion::IdentityCredentialPayload(
      credential.idp_config_url, credential.account_id, credential.fields);
  suggestion.main_text = Suggestion::Text(credential.main_text);
  suggestion.custom_icon = credential.custom_icon;
  // TODO(crbug.com/410421491): support more context.
  suggestion.labels.push_back({Suggestion::Text(
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_IDENTITY_CREDENTIAL_LABEL_TEXT,
                                 credential.idp_for_display))});
  return suggestion;
}

}  // namespace

IdentityCredentialSuggestionGenerator::IdentityCredentialSuggestionGenerator(
    base::RepeatingCallback<content::webid::AutofillSource*()> source)
    : source_(source) {}

IdentityCredentialSuggestionGenerator::
    ~IdentityCredentialSuggestionGenerator() = default;

void IdentityCredentialSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void IdentityCredentialSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  if (!trigger_autofill_field) {
    callback({SuggestionDataSource::kIdentityCredential, {}});
    return;
  }
  FieldType trigger_field_type =
      trigger_autofill_field->Type().GetIdentityCredentialType();

  // Identity credential suggestions are only supported on PASSWORD fields.
  if (trigger_field_type != PASSWORD) {
    callback({SuggestionDataSource::kIdentityCredential, {}});
    return;
  }

  if (SuppressSuggestionsForAutocompleteUnrecognizedField(
          *trigger_autofill_field, GetAcUnrecognizedBehavior(client))) {
    callback({SuggestionDataSource::kIdentityCredential, {}});
    return;
  }

  content::webid::AutofillSource* source = source_.Run();

  if (!source) {
    callback({SuggestionDataSource::kIdentityCredential, {}});
    return;
  }

  std::optional<std::vector<IdentityRequestAccountPtr>> accounts =
      source->GetAutofillSuggestions();
  if (!accounts) {
    callback({SuggestionDataSource::kIdentityCredential, {}});
    return;
  }

  std::vector<IdentityCredential> credentials;
  for (IdentityRequestAccountPtr account : *accounts) {
    bool is_returning_credential =
        account->idp_claimed_login_state.value_or(
            account->browser_trusted_login_state) ==
        content::IdentityRequestAccount::LoginState::kSignIn;
    if (!is_returning_credential) {
      continue;
    }
    credentials.emplace_back(
        account->identity_provider->idp_metadata.config_url, account->id,
        base::UTF8ToUTF16(account->identity_provider->idp_for_display),
        base::UTF8ToUTF16(account->email),
        std::map<FieldType, std::u16string>(), account->decoded_picture);
  }

  callback(
      {SuggestionDataSource::kIdentityCredential,
       base::ToVector(credentials, [&](const IdentityCredential& credential) {
         return CreateIdentityCredentialSuggestion(credential,
                                                   trigger_field_type);
       })});
}

}  // namespace autofill
