// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/suggestions/identity_credential_suggestion_generator.h"

#include "base/containers/contains.h"
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
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

std::map<FieldType, std::u16string> CreateFederatedProfileFields(
    IdentityRequestAccountPtr account) {
  std::map<FieldType, std::u16string> fields;

  if (!account->email.empty() &&
      base::Contains(account->identity_provider->disclosure_fields,
                     content::IdentityRequestDialogDisclosureField::kEmail)) {
    fields[EMAIL_ADDRESS] = base::UTF8ToUTF16(account->email);
  }

  if (!account->name.empty() &&
      base::Contains(account->identity_provider->disclosure_fields,
                     content::IdentityRequestDialogDisclosureField::kName)) {
    fields[NAME_FULL] = base::UTF8ToUTF16(account->name);
  }

  if (!account->phone.empty() &&
      base::Contains(
          account->identity_provider->disclosure_fields,
          content::IdentityRequestDialogDisclosureField::kPhoneNumber)) {
    fields[PHONE_HOME_WHOLE_NUMBER] = base::UTF8ToUTF16(account->phone);
  }

  return fields;
}

Suggestion CreateIdentityCredentialSuggestion(
    const IdentityCredential& credential,
    FieldType field_type) {
  CHECK(field_type == PASSWORD || credential.fields.contains(field_type));
  Suggestion suggestion(SuggestionType::kIdentityCredential);
  suggestion.payload = Suggestion::IdentityCredentialPayload(
      credential.idp_config_url, credential.account_id, credential.fields);

  switch (field_type) {
    case EMAIL_ADDRESS:
      suggestion.main_text =
          Suggestion::Text(credential.fields.at(EMAIL_ADDRESS));
      suggestion.icon = Suggestion::Icon::kEmail;
      suggestion.minor_texts.emplace_back(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_IDENTITY_CREDENTIAL_VERIFIED_MINOR_TEXT,
          credential.idp_for_display));
      suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_IDENTITY_CREDENTIAL_VERIFIED_EMAIL_LABEL))});
      break;
    case PASSWORD:
      suggestion.main_text = Suggestion::Text(credential.main_text);
      suggestion.custom_icon = credential.custom_icon;
      // TODO(crbug.com/410421491): support more context.
      suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_IDENTITY_CREDENTIAL_LABEL_TEXT,
          credential.idp_for_display))});
      break;
    case NAME_FULL:
    case PHONE_HOME_WHOLE_NUMBER:
      suggestion.main_text = Suggestion::Text(credential.fields.at(field_type));
      suggestion.icon = Suggestion::Icon::kAccount;
      suggestion.minor_texts.emplace_back(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_IDENTITY_CREDENTIAL_PROVIDED_MINOR_TEXT,
          credential.idp_for_display));
      break;
    case NAME_FIRST:
    case UNKNOWN_TYPE:
      // We should not reach this case since an early return in
      // `IdentityCredentialSuggestionGenerator::FetchSuggestionData()` should
      // prevent this.
      // TODO(crbug.com/380367784): Add support for suggestions on NAME_FIRST
      // fields.
    default:
      // The given `field_type` must be one of the IdentityCredential types
      // in the co-domain of AutofillType::GetIdentityCredentialType().
      NOTREACHED();
  }

  return suggestion;
}

}  // namespace

IdentityCredentialSuggestionGenerator::IdentityCredentialSuggestionGenerator(
    base::RepeatingCallback<content::webid::AutofillSource*()> source)
    : source_(source) {}

IdentityCredentialSuggestionGenerator::
    ~IdentityCredentialSuggestionGenerator() = default;

void IdentityCredentialSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](std::pair<SuggestionDataSource,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void IdentityCredentialSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void IdentityCredentialSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  if (!trigger_autofill_field) {
    callback({SuggestionDataSource::kIdentityCredential, {}});
    return;
  }
  trigger_field_type_ =
      trigger_autofill_field->Type().GetIdentityCredentialType();

  // TODO(crbug.com/380367784): Add support for suggestions on NAME_FIRST
  // fields.
  if (trigger_field_type_ == UNKNOWN_TYPE ||
      trigger_field_type_ == NAME_FIRST) {
    callback({SuggestionDataSource::kIdentityCredential, {}});
    return;
  }

  if (SuppressSuggestionsForAutocompleteUnrecognizedField(
          *trigger_autofill_field)) {
    callback({SuggestionDataSource::kIdentityCredential, {}});
    return;
  }

  // TODO(crbug.com/380367784): reproduce and add a test to make sure this
  // works properly when FedCM is called from inner frames.
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

  std::vector<SuggestionData> suggestion_data;
  for (IdentityRequestAccountPtr account : *accounts) {
    bool delegated =
        account->identity_provider->format &&
        *account->identity_provider->format == blink::mojom::Format::kSdJwt;
    bool is_returning_credential =
        account->idp_claimed_login_state.value_or(
            account->browser_trusted_login_state) ==
        content::IdentityRequestAccount::LoginState::kSignIn;
    if (!delegated && !is_returning_credential) {
      continue;
    }
    if (IdentityCredential credentials(
            account->identity_provider->idp_metadata.config_url, account->id,
            base::UTF8ToUTF16(account->identity_provider->idp_for_display),
            base::UTF8ToUTF16(account->email),
            CreateFederatedProfileFields(account), account->decoded_picture);
        trigger_field_type_ == PASSWORD ||
        credentials.fields.contains(trigger_field_type_)) {
      suggestion_data.push_back(std::move(credentials));
    }
  }
  callback(
      {SuggestionDataSource::kIdentityCredential, std::move(suggestion_data)});
}

void IdentityCredentialSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  auto it = all_suggestion_data.find(SuggestionDataSource::kIdentityCredential);
  std::vector<SuggestionData> identity_credential_suggestion_data =
      it != all_suggestion_data.end() ? it->second
                                      : std::vector<SuggestionData>();
  if (identity_credential_suggestion_data.empty()) {
    callback({FillingProduct::kAutocomplete, {}});
    return;
  }
  if (!trigger_autofill_field) {
    callback({FillingProduct::kIdentityCredential, {}});
    return;
  }

  std::vector<IdentityCredential> credentials = base::ToVector(
      std::move(identity_credential_suggestion_data),
      [](SuggestionData& suggestion_data) {
        return std::get<IdentityCredential>(std::move(suggestion_data));
      });

  callback(
      {FillingProduct::kIdentityCredential,
       base::ToVector(credentials, [&](const IdentityCredential& credential) {
         return CreateIdentityCredentialSuggestion(credential,
                                                   trigger_field_type_);
       })});
}

}  // namespace autofill
