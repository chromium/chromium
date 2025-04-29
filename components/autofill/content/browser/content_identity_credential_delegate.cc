// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_identity_credential_delegate.h"

#include "base/functional/callback.h"
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
  if (!(field_type == PASSWORD || field_type == EMAIL_ADDRESS ||
        field_type == NAME_FIRST || field_type == NAME_FULL)) {
    return {};
  }
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
    Suggestion suggestion(SuggestionType::kIdentityCredential);
    auto payload = Suggestion::IdentityCredentialPayload(
        account->identity_provider->idp_metadata.config_url, account->id);

    if (field_type == EMAIL_ADDRESS || field_type == NAME_FIRST ||
        field_type == NAME_FULL) {
      payload.fields[NAME_FULL] = base::UTF8ToUTF16(account->name);
      payload.fields[NAME_FIRST] = base::UTF8ToUTF16(account->given_name);
      payload.fields[EMAIL_ADDRESS] = base::UTF8ToUTF16(account->email);

      suggestion.main_text = Suggestion::Text(payload.fields[field_type]);
      // TODO(crbug.com/380367784): revisit the iconography of the suggestion
      // if the field goes beyond email.
      suggestion.icon = Suggestion::Icon::kEmail;
      suggestion.minor_texts.emplace_back(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_IDENTITY_CREDENTIAL_MINOR_TEXT,
          base::UTF8ToUTF16(account->identity_provider->idp_for_display)));
      suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_IDENTITY_CREDENTIAL_EMAIL_LABEL))});
    } else if (field_type == PASSWORD) {
      suggestion.main_text =
          Suggestion::Text(base::UTF8ToUTF16(account->email));
      suggestion.custom_icon = account->decoded_picture;
      // TODO(crbug.com/410421491): support more context.
      suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_IDENTITY_CREDENTIAL_LABEL_TEXT,
          base::UTF8ToUTF16(account->identity_provider->idp_for_display)))});
    }

    suggestion.payload = payload;
    suggestions.push_back(std::move(suggestion));
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
