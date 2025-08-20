// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_identity_credential_delegate.h"

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/identity_credential/identity_credential.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
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

std::optional<Suggestion> CreateVerifiedEmailSuggestion(
    IdentityCredential& credential) {
  if (!credential.fields.contains(EMAIL_ADDRESS)) {
    return std::nullopt;
  }

  Suggestion suggestion(SuggestionType::kIdentityCredential);
  suggestion.main_text = Suggestion::Text(credential.fields[EMAIL_ADDRESS]);
  suggestion.icon = Suggestion::Icon::kEmail;
  suggestion.minor_texts.emplace_back(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_IDENTITY_CREDENTIAL_VERIFIED_MINOR_TEXT,
      credential.idp_for_display));
  suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_IDENTITY_CREDENTIAL_VERIFIED_EMAIL_LABEL))});
  suggestion.payload = Suggestion::IdentityCredentialPayload(
      credential.idp_config_url, credential.account_id, credential.fields);

  return suggestion;
}

Suggestion CreatePasswordSuggestion(IdentityCredential& credential) {
  Suggestion suggestion(SuggestionType::kIdentityCredential);

  suggestion.main_text = Suggestion::Text(credential.main_text);
  suggestion.custom_icon = credential.custom_icon;
  // TODO(crbug.com/410421491): support more context.
  suggestion.labels.push_back({Suggestion::Text(
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_IDENTITY_CREDENTIAL_LABEL_TEXT,
                                 credential.idp_for_display))});
  suggestion.payload = Suggestion::IdentityCredentialPayload(
      credential.idp_config_url, credential.account_id, credential.fields);

  return suggestion;
}

std::optional<Suggestion> CreateProvidedFieldSuggestion(
    IdentityCredential& credential,
    const FieldType& field_type) {
  if (!credential.fields.contains(field_type)) {
    return std::nullopt;
  }

  Suggestion suggestion(SuggestionType::kIdentityCredential);
  suggestion.main_text = Suggestion::Text(credential.fields[field_type]);
  suggestion.icon = Suggestion::Icon::kAccount;
  suggestion.minor_texts.emplace_back(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_IDENTITY_CREDENTIAL_PROVIDED_MINOR_TEXT,
      credential.idp_for_display));
  suggestion.payload = Suggestion::IdentityCredentialPayload(
      credential.idp_config_url, credential.account_id, credential.fields);

  return suggestion;
}

}  // namespace

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
  if (!autofill_field) {
    return {};
  }

  // TODO(crbug.com/380367784): reproduce and add a test to make sure this
  // works properly when FedCM is called from inner frames.
  content::webid::AutofillSource* source = source_.Run();

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
    IdentityCredential identity_credential = IdentityCredential(
        account->identity_provider->idp_metadata.config_url, account->id,
        base::UTF8ToUTF16(account->identity_provider->idp_for_display),
        base::UTF8ToUTF16(account->email),
        CreateFederatedProfileFields(account), account->decoded_picture);
    switch (autofill_field->Type().GetIdentityCredentialType()) {
      case EMAIL_ADDRESS: {
        if (std::optional<Suggestion> suggestion =
                CreateVerifiedEmailSuggestion(identity_credential);
            suggestion) {
          suggestions.emplace_back(std::move(*suggestion));
        }
        break;
      }
      case NAME_FULL:
        [[fallthrough]];  // Intentional fall through.
      case PHONE_HOME_WHOLE_NUMBER: {
        if (std::optional<Suggestion> suggestion =
                CreateProvidedFieldSuggestion(
                    identity_credential,
                    autofill_field->Type().GetIdentityCredentialType());
            suggestion) {
          suggestions.emplace_back(std::move(*suggestion));
        }
        break;
      }
      case PASSWORD: {
        suggestions.emplace_back(CreatePasswordSuggestion(identity_credential));
        break;
      }
      case UNKNOWN_TYPE: {
        // Unsupported field type.
        return {};
      }
      default: {
        // The given `field_type` must be one of the Identity Credentials types
        // in the co-domain of AutofillType::GetIdentityCredentialType().
        NOTREACHED();
      }
    }
  }

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

}  // namespace autofill
