// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class OtpManager;

// Generates OTP suggestions from the provided vector of retrieved OTP values.
// TODO(crbug.com/409962888): Cleanup once AutofillNewSuggestionGeneration is
// launched.
std::vector<Suggestion> BuildOtpSuggestions(
    std::vector<std::string> one_time_passwords,
    const FieldGlobalId& field_id);

// A `SuggestionGenerator` for `FillingProduct::kOneTimePassword`.
class OtpSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit OtpSuggestionGenerator(OtpManager& otp_manager);
  ~OtpSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

 private:
  void OnOtpReturned(
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback,
      std::vector<std::string> one_time_passwords);

  const base::raw_ref<OtpManager> otp_manager_;

  base::WeakPtrFactory<OtpSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_
