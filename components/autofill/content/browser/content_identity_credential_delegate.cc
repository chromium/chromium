// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_identity_credential_delegate.h"

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/federated_auth_autofill_source.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

Suggestion::IdentityCredentialPayload CreateFederatedProfile(
    IdentityRequestAccountPtr account) {
  Suggestion::IdentityCredentialPayload profile =
      Suggestion::IdentityCredentialPayload(
          account->identity_provider->idp_metadata.config_url, account->id);

  if (!account->email.empty() &&
      base::Contains(account->identity_provider->disclosure_fields,
                     content::IdentityRequestDialogDisclosureField::kEmail)) {
    profile.fields[EMAIL_ADDRESS] = base::UTF8ToUTF16(account->email);
  }

  if (!account->name.empty() &&
      base::Contains(account->identity_provider->disclosure_fields,
                     content::IdentityRequestDialogDisclosureField::kName)) {
    profile.fields[NAME_FULL] = base::UTF8ToUTF16(account->name);
  }

  if (!account->phone.empty() &&
      base::Contains(
          account->identity_provider->disclosure_fields,
          content::IdentityRequestDialogDisclosureField::kPhoneNumber)) {
    profile.fields[PHONE_HOME_WHOLE_NUMBER] = base::UTF8ToUTF16(account->phone);
  }

  return profile;
}

std::optional<Suggestion> CreateVerifiedEmailSuggestion(
    IdentityRequestAccountPtr account) {
  Suggestion::IdentityCredentialPayload profile =
      CreateFederatedProfile(account);

  if (!profile.fields.contains(EMAIL_ADDRESS)) {
    return std::nullopt;
  }

  Suggestion suggestion(SuggestionType::kIdentityCredential);
  suggestion.main_text = Suggestion::Text(profile.fields[EMAIL_ADDRESS]);
  suggestion.icon = Suggestion::Icon::kEmail;
  suggestion.minor_texts.emplace_back(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_IDENTITY_CREDENTIAL_VERIFIED_MINOR_TEXT,
      base::UTF8ToUTF16(account->identity_provider->idp_for_display)));
  suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_IDENTITY_CREDENTIAL_VERIFIED_EMAIL_LABEL))});
  suggestion.payload = profile;

  return suggestion;
}

Suggestion CreatePasswordSuggestion(IdentityRequestAccountPtr account) {
  Suggestion suggestion(SuggestionType::kIdentityCredential);

  suggestion.main_text = Suggestion::Text(base::UTF8ToUTF16(account->email));
  suggestion.custom_icon = account->decoded_picture;
  // TODO(crbug.com/410421491): support more context.
  suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_IDENTITY_CREDENTIAL_LABEL_TEXT,
      base::UTF8ToUTF16(account->identity_provider->idp_for_display)))});
  suggestion.payload = CreateFederatedProfile(account);

  return suggestion;
}

std::optional<Suggestion> CreateProvidedFieldSuggestion(
    IdentityRequestAccountPtr account,
    const FieldType& field_type) {
  Suggestion::IdentityCredentialPayload profile =
      CreateFederatedProfile(account);

  if (!profile.fields.contains(field_type)) {
    return std::nullopt;
  }

  Suggestion suggestion(SuggestionType::kIdentityCredential);
  suggestion.main_text = Suggestion::Text(profile.fields[field_type]);
  suggestion.icon = Suggestion::Icon::kAccount;
  suggestion.minor_texts.emplace_back(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_IDENTITY_CREDENTIAL_PROVIDED_MINOR_TEXT,
      base::UTF8ToUTF16(account->identity_provider->idp_for_display)));
  suggestion.payload = profile;

  return suggestion;
}

}  // namespace

ContentIdentityCredentialDelegate::ContentIdentityCredentialDelegate(
    content::WebContents* web_contents)
    : ContentIdentityCredentialDelegate(base::BindRepeating(
          [](content::WebContents* web_contents)
              -> content::FederatedAuthAutofillSource* {
            return content::FederatedAuthAutofillSource::FromPage(
                web_contents->GetPrimaryPage());
          },
          web_contents)) {}

ContentIdentityCredentialDelegate::ContentIdentityCredentialDelegate(
    base::RepeatingCallback<content::FederatedAuthAutofillSource*()> source)
    : source_(std::move(source)) {}

ContentIdentityCredentialDelegate::~ContentIdentityCredentialDelegate() =
    default;

std::vector<Suggestion>
ContentIdentityCredentialDelegate::GetVerifiedAutofillSuggestions(
    const FieldType& field_type) const {
  // TODO(crbug.com/380367784): reproduce and add a test to make sure this
  // works properly when FedCM is called from inner frames.
  content::FederatedAuthAutofillSource* source = source_.Run();

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
        account->login_state &&
        *account->login_state ==
            content::IdentityRequestAccount::LoginState::kSignIn;
    if (!delegated && !is_returning_credential) {
      continue;
    }

    switch (field_type) {
      case EMAIL_ADDRESS: {
        if (std::optional<Suggestion> suggestion =
                CreateVerifiedEmailSuggestion(account);
            suggestion) {
          suggestions.emplace_back(std::move(*suggestion));
        }
        break;
      }
      case NAME_FULL:
        [[fallthrough]];  // Intentional fall through.
      case PHONE_HOME_WHOLE_NUMBER: {
        if (std::optional<Suggestion> suggestion =
                CreateProvidedFieldSuggestion(account, field_type);
            suggestion) {
          suggestions.emplace_back(std::move(*suggestion));
        }
        break;
      }
      case PASSWORD: {
        suggestions.emplace_back(CreatePasswordSuggestion(account));
        break;
      }
      default:
        // Unsupported field type.
        return {};
    }
  }

  return suggestions;
}

void ContentIdentityCredentialDelegate::NotifySuggestionAccepted(
    const Suggestion& suggestion,
    bool show_modal,
    OnFederatedTokenReceivedCallback callback) const {
  content::FederatedAuthAutofillSource* source = source_.Run();

  if (!source) {
    return;
  }

  Suggestion::IdentityCredentialPayload payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();

  source->NotifyAutofillSuggestionAccepted(
      payload.config_url, payload.account_id, show_modal, std::move(callback));
}

}  // namespace autofill
