// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
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
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Builds Suggestion for given `otp_value`.
Suggestion BuildOtpSuggestion(const std::string& otp_value,
                              SuggestionType type,
                              std::string_view sender_address) {
  Suggestion suggestion(base::UTF8ToUTF16(otp_value), type);
  switch (type) {
    case SuggestionType::kGmailOneTimePasswordEntry: {
      suggestion.icon = Suggestion::Icon::kMailAsterisk;
      suggestion.minor_texts = {Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_GMAIL_OTP_VERIFICATION_CODE_LABEL))};
      std::u16string sender_address_16 = base::UTF8ToUTF16(sender_address);
      suggestion.voice_over = l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_GMAIL_ONE_TIME_PASSWORD_VOICE_OVER_A11Y_LABEL,
          suggestion.main_text.value, sender_address_16);
      suggestion.labels = {{Suggestion::Text(std::move(sender_address_16))}};
      break;
    }
    case SuggestionType::kOneTimePasswordEntry:
#if BUILDFLAG(IS_ANDROID)
      suggestion.icon = Suggestion::Icon::kAndroidMessages;
#endif
      suggestion.voice_over = l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_ONE_TIME_PASSWORD_VOICE_OVER_A11Y_LABEL,
          suggestion.main_text.value);
      break;
    default:
      NOTREACHED();
  }
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_ONE_TIME_PASSWORD);
  return suggestion;
}

// Returns `value` without whitespace and ASCII punctuation (e.g. separators
// like '-' or input mask placeholders like '_') and with ASCII letters in
// lower case, so that OTPs can be compared independently of their formatting.
std::u16string NormalizeOtpForComparison(std::u16string_view value) {
  std::u16string normalized;
  normalized.reserve(value.size());
  for (char16_t c : value) {
    if (!base::IsUnicodeWhitespace(c) && !base::IsAsciiPunctuation(c)) {
      normalized.push_back(base::ToLowerASCII(c));
    }
  }
  return normalized;
}

}  // namespace

std::vector<Suggestion> BuildOtpSuggestions(
    base::span<const one_time_tokens::OneTimeToken> one_time_tokens) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(one_time_tokens.size() + 2);
  bool has_gmail_otp = false;
  for (const one_time_tokens::OneTimeToken& token : one_time_tokens) {
    const SuggestionType type =
        token.type() == one_time_tokens::OneTimeTokenType::kGmail
            ? SuggestionType::kGmailOneTimePasswordEntry
            : SuggestionType::kOneTimePasswordEntry;
    std::string_view sender_address =
        token.sender_address() ? *token.sender_address() : std::string_view();
    if (type == SuggestionType::kGmailOneTimePasswordEntry) {
      if (sender_address.empty()) {
        continue;
      }
      has_gmail_otp = true;
    }
    suggestions.push_back(
        BuildOtpSuggestion(token.value(), type, sender_address));
  }
  if (has_gmail_otp) {
    suggestions.emplace_back(SuggestionType::kSeparator);
    Suggestion& open_gmail = suggestions.emplace_back(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_OPEN_GMAIL_FOR_OTP),
        SuggestionType::kOpenGmailForOtps);
    open_gmail.icon = Suggestion::Icon::kGmail;
    open_gmail.trailing_icon = Suggestion::Icon::kOpenInNew;
  }
  return suggestions;
}

std::vector<one_time_tokens::OneTimeToken> GetOtpsToSuggest(
    std::vector<one_time_tokens::OneTimeToken> one_time_tokens,
    const FormFieldData& trigger_field) {
  const std::u16string field_value =
      NormalizeOtpForComparison(trigger_field.value());
  if (field_value.empty()) {
    return one_time_tokens;
  }
  std::erase_if(
      one_time_tokens, [&](const one_time_tokens::OneTimeToken& token) {
        const std::u16string otp_value =
            NormalizeOtpForComparison(base::UTF8ToUTF16(token.value()));
        return otp_value == field_value ||
               (!trigger_field.is_autofilled_according_to_renderer() &&
                !otp_value.starts_with(field_value));
      });
  return one_time_tokens;
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
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     trigger_field));
}

void OtpSuggestionGenerator::OnOtpReturned(
    base::OnceCallback<void(ReturnedSuggestions)> callback,
    const FormFieldData& trigger_field,
    std::vector<one_time_tokens::OneTimeToken> one_time_tokens) {
  std::move(callback).Run({SuggestionDataSource::kOneTimePassword,
                           BuildOtpSuggestions(GetOtpsToSuggest(
                               std::move(one_time_tokens), trigger_field))});
}

}  // namespace autofill
