// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "components/autofill/core/browser/suggestions/one_time_passwords/one_time_password_suggestion_data.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Builds Suggestion for given `otp_value`.
Suggestion BuildOtpSuggestion(const std::string& otp_value,
                              const FieldGlobalId& field_id) {
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
    std::vector<std::string> one_time_passwords,
    const FieldGlobalId& field_id) {
  std::vector<Suggestion> suggestions;
  for (const std::string& otp_value : one_time_passwords) {
    suggestions.push_back(BuildOtpSuggestion(otp_value, field_id));
  }
  return suggestions;
}

OtpSuggestionGenerator::OtpSuggestionGenerator(OtpManager& otp_manager)
    : otp_manager_(otp_manager) {}

OtpSuggestionGenerator::~OtpSuggestionGenerator() = default;

void OtpSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
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

void OtpSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  auto it = all_suggestion_data.find(SuggestionDataSource::kOneTimePassword);
  std::vector<SuggestionData> otp_suggestion_data =
      it != all_suggestion_data.end() ? it->second
                                      : std::vector<SuggestionData>();
  if (otp_suggestion_data.empty()) {
    std::move(callback).Run({FillingProduct::kOneTimePassword, {}});
    return;
  }

  std::vector<std::string> one_time_passwords = base::ToVector(
      std::move(otp_suggestion_data), [](SuggestionData& suggestion_data) {
        return *std::get<OneTimePasswordSuggestionData>(
            std::move(suggestion_data));
      });

  std::move(callback).Run(
      {FillingProduct::kOneTimePassword,
       BuildOtpSuggestions(one_time_passwords, trigger_field.global_id())});
}

void OtpSuggestionGenerator::OnOtpReturned(
    base::OnceCallback<void(
        std::pair<SuggestionDataSource,
                  std::vector<SuggestionGenerator::SuggestionData>>)> callback,
    std::vector<std::string> one_time_passwords) {
  std::vector<SuggestionGenerator::SuggestionData> suggestion_data =
      base::ToVector(
          std::move(one_time_passwords), [](std::string& one_time_password) {
            return SuggestionGenerator::SuggestionData(
                OneTimePasswordSuggestionData(std::move(one_time_password)));
          });
  std::move(callback).Run(
      {SuggestionDataSource::kOneTimePassword, std::move(suggestion_data)});
}

}  // namespace autofill
