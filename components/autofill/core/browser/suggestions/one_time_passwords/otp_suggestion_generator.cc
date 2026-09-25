// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Builds Suggestion for given `otp_value`.
Suggestion BuildOtpSuggestion(const std::string& otp_value,
                              SuggestionType type,
                              std::string_view account_email) {
  Suggestion suggestion(base::UTF8ToUTF16(otp_value), type);
  if (type == SuggestionType::kGmailOneTimePasswordEntry) {
    suggestion.icon = Suggestion::Icon::kMailAsterisk;
    suggestion.minor_texts = {Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_GMAIL_OTP_VERIFICATION_CODE_LABEL))};
    suggestion.labels = {{Suggestion::Text(
        l10n_util::GetStringFUTF16(IDS_AUTOFILL_GMAIL_OTP_FROM_ACCOUNT_LABEL,
                                   base::UTF8ToUTF16(account_email)))}};
  }
#if BUILDFLAG(IS_ANDROID)
  // Choose the right icon and A11Y label when more OTP options are supported
  // in the future.
  if (type == SuggestionType::kOneTimePasswordEntry) {
    suggestion.icon = Suggestion::Icon::kAndroidMessages;
  }
#endif
  suggestion.voice_over = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_ONE_TIME_PASSWORD_VOICE_OVER_A11Y_LABEL,
      suggestion.main_text.value);
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_ONE_TIME_PASSWORD);
  return suggestion;
}

}  // namespace

std::vector<Suggestion> BuildOtpSuggestions(
    base::span<const std::string> one_time_passwords,
    SuggestionType type,
    std::string_view account_email) {
  CHECK(type == SuggestionType::kGmailOneTimePasswordEntry ||
        type == SuggestionType::kOneTimePasswordEntry);
  if (one_time_passwords.empty() ||
      (type == SuggestionType::kGmailOneTimePasswordEntry &&
       account_email.empty())) {
    return {};
  }
  std::vector<Suggestion> suggestions;
  suggestions.reserve(
      one_time_passwords.size() +
      (type == SuggestionType::kGmailOneTimePasswordEntry ? 2 : 0));
  for (const std::string& otp_value : one_time_passwords) {
    suggestions.push_back(BuildOtpSuggestion(otp_value, type, account_email));
  }
  if (type == SuggestionType::kGmailOneTimePasswordEntry) {
    suggestions.emplace_back(SuggestionType::kSeparator);
    Suggestion& open_gmail = suggestions.emplace_back(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_OPEN_GMAIL_FOR_OTP),
        SuggestionType::kOpenGmailForOtps);
    open_gmail.icon = Suggestion::Icon::kGmail;
    open_gmail.trailing_icon = Suggestion::Icon::kOpenInNew;
  }
  return suggestions;
}

OtpSuggestionGenerator::OtpSuggestionGenerator(OtpManager& otp_manager)
    : otp_manager_(otp_manager) {}

OtpSuggestionGenerator::~OtpSuggestionGenerator() = default;

void OtpSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  if (!form_structure || !trigger_autofill_field) {
    std::move(callback).Run({SuggestionDataSource::kOneTimePassword, {}});
    return;
  }

  if (!trigger_autofill_field->Type().GetTypes().contains(ONE_TIME_CODE)) {
    std::move(callback).Run({SuggestionDataSource::kOneTimePassword, {}});
    return;
  }

  if (!client.IsContextSecure()) {
    std::move(callback).Run({SuggestionDataSource::kOneTimePassword, {}});
    return;
  }

  otp_manager_->GetOtpSuggestions(
      *form_structure, trigger_field,
      base::BindOnce(&OtpSuggestionGenerator::OnOtpReturned,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OtpSuggestionGenerator::OnOtpReturned(
    base::OnceCallback<void(ReturnedSuggestions)> callback,
    std::vector<std::string> one_time_passwords) {
  // TODO(crbug.com/565217441): Pass
  // `SuggestionType::kGmailOneTimePasswordEntry` and the signed-in primary
  // account email from `IdentityManager` once `OtpManager` distinguishes Gmail
  // vs. SMS OTP tokens.
  std::move(callback).Run({SuggestionDataSource::kOneTimePassword,
                           BuildOtpSuggestions(one_time_passwords)});
}

}  // namespace autofill
