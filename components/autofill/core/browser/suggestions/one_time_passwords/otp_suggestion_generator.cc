// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/strings/grit/components_strings.h"
#endif
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace autofill {
namespace {

// Builds Suggestion for given `otp_value`.
Suggestion BuildOtpSuggestion(const std::string& otp_value) {
  Suggestion suggestion = Suggestion(base::UTF8ToUTF16(otp_value),
                                     SuggestionType::kOneTimePasswordEntry);
#if BUILDFLAG(IS_ANDROID)
  // Android SMS OTPs are the only supported OTPs at the moment. Choose the
  // right icon and A11Y label when more OTP options are supported in the
  // future.
  suggestion.icon = Suggestion::Icon::kAndroidMessages;
  suggestion.voice_over = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_ONE_TIME_PASSWORD_VOICE_OVER_A11Y_LABEL,
      base::UTF8ToUTF16(otp_value));
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_ONE_TIME_PASSWORD);
#endif
  return suggestion;
}

}  // namespace

std::vector<Suggestion> BuildOtpSuggestions(
    std::vector<std::string> one_time_passwords) {
  std::vector<Suggestion> suggestions;
  for (const std::string& otp_value : one_time_passwords) {
    suggestions.push_back(BuildOtpSuggestion(otp_value));
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

  otp_manager_->GetOtpSuggestions(
      base::BindOnce(&OtpSuggestionGenerator::OnOtpReturned,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OtpSuggestionGenerator::OnOtpReturned(
    base::OnceCallback<void(ReturnedSuggestions)> callback,
    std::vector<std::string> one_time_passwords) {
  std::move(callback).Run({SuggestionDataSource::kOneTimePassword,
                           BuildOtpSuggestions(one_time_passwords)});
}

}  // namespace autofill
