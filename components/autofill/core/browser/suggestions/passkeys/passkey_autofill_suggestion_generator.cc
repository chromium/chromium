// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/passkeys/passkey_autofill_suggestion_generator.h"

#include "base/feature_list.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/autofill/core/browser/suggestions/passkeys/hybrid_passkey_availability.h"
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

PasskeyAutofillSuggestionGenerator::PasskeyAutofillSuggestionGenerator(
    PasswordManagerDelegate& password_manager_delegate)
    : password_manager_delegate_(password_manager_delegate) {}
PasskeyAutofillSuggestionGenerator::~PasskeyAutofillSuggestionGenerator() =
    default;

void PasskeyAutofillSuggestionGenerator::FetchSuggestionData(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  if (!ShouldShowWebauthnHybridEntryPoint(field_data) ||
      !password_manager_delegate_
           ->GetWebauthnSignInWithAnotherDeviceSuggestion()) {
    std::move(callback).Run({FillingProduct::kPasskey, {}});
    return;
  }
  std::move(callback).Run(
      {FillingProduct::kPasskey, {HybridPasskeyAvailability(true)}});
}

void PasskeyAutofillSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<
        std::pair<FillingProduct,
                  std::vector<SuggestionGenerator::SuggestionData>>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  std::vector<Suggestion> suggestions;
  const std::vector<SuggestionData>& passkey_data =
      ExtractSuggestionDataForFillingProduct(all_suggestion_data,
                                             FillingProduct::kPasskey);

  if (!passkey_data.empty()) {
    CHECK_EQ(passkey_data.size(), 1u);
    CHECK(std::holds_alternative<HybridPasskeyAvailability>(passkey_data[0]));
    CHECK(std::get<HybridPasskeyAvailability>(passkey_data[0]).value());

    if (auto suggestion =
            password_manager_delegate_
                ->GetWebauthnSignInWithAnotherDeviceSuggestion()) {
      suggestions.push_back(*std::move(suggestion));
    }
  }
  std::move(callback).Run(
      std::make_pair(FillingProduct::kPasskey, std::move(suggestions)));
}

}  // namespace autofill
