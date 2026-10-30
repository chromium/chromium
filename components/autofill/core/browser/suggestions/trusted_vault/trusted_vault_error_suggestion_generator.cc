// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/trusted_vault/trusted_vault_error_suggestion_generator.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

TrustedVaultErrorSuggestionGenerator::TrustedVaultErrorSuggestionGenerator() =
    default;
TrustedVaultErrorSuggestionGenerator::~TrustedVaultErrorSuggestionGenerator() =
    default;

void TrustedVaultErrorSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  std::move(callback).Run(
      {SuggestionDataSource::kTrustedVaultError, {}});
#else
  const PasswordManagerDelegate* password_delegate =
      client.GetPasswordManagerDelegate(trigger_field.global_id());
  if (!password_delegate) {
    std::move(callback).Run(
        {SuggestionDataSource::kTrustedVaultError, {}});
    return;
  }
  std::vector<Suggestion> suggestions;
  if (std::optional<Suggestion> unlock_suggestion =
          password_delegate->GetTrustedVaultUnlockSuggestion()) {
    suggestions.push_back(*std::move(unlock_suggestion));
  }
  std::move(callback).Run(
      {SuggestionDataSource::kTrustedVaultError, std::move(suggestions)});
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

}  // namespace autofill
