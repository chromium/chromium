// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PASSKEYS_PASSKEY_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PASSKEYS_PASSKEY_SUGGESTION_GENERATOR_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class PasswordManagerDelegate;

// Responsible for generating password-related suggestions. Currently, it only
// provides a suggestion to sign in with a passkey from another device.
class PasskeySuggestionGenerator : public SuggestionGenerator {
 public:
  explicit PasskeySuggestionGenerator(
      PasswordManagerDelegate& password_manager_delegate);
  ~PasskeySuggestionGenerator() override;

  // SuggestionGenerator:
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
  const raw_ref<PasswordManagerDelegate> password_manager_delegate_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PASSKEYS_PASSKEY_SUGGESTION_GENERATOR_H_
