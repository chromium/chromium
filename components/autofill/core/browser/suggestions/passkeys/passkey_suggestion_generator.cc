// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/passkeys/passkey_suggestion_generator.h"

#include "base/feature_list.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

bool ShouldShowWebauthnHybridEntryPoint(const FormFieldData& field) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  const std::optional<autofill::AutocompleteParsingResult>& autocomplete =
      field.parsed_autocomplete();
  return autocomplete.has_value() &&  // Assume no autcomplete if not parsed.
         autocomplete->webauthn &&    // Field must have "webauthn" annotation.
         base::FeatureList::IsEnabled(
             password_manager::features::
                 kAutofillReintroduceHybridPasskeyDropdownItem);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

}  // namespace

PasskeySuggestionGenerator::PasskeySuggestionGenerator(
    PasswordManagerDelegate& password_manager_delegate)
    : password_manager_delegate_(password_manager_delegate) {}
PasskeySuggestionGenerator::~PasskeySuggestionGenerator() = default;

void PasskeySuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  if (!ShouldShowWebauthnHybridEntryPoint(trigger_field)) {
    std::move(callback).Run({SuggestionDataSource::kPasskey, {}});
    return;
  }
  std::vector<Suggestion> suggestions;
  if (std::optional<Suggestion> suggestion =
          password_manager_delegate_
              ->GetWebauthnSignInWithAnotherDeviceSuggestion()) {
    suggestions.push_back(*std::move(suggestion));
  }
  std::move(callback).Run(
      {SuggestionDataSource::kPasskey, std::move(suggestions)});
}

}  // namespace autofill
