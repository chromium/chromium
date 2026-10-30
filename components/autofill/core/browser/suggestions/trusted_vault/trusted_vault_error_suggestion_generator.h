// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_TRUSTED_VAULT_TRUSTED_VAULT_ERROR_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_TRUSTED_VAULT_TRUSTED_VAULT_ERROR_SUGGESTION_GENERATOR_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class FormFieldData;

// Responsible for generating suggestions for resolving a Trusted Vault error.
class TrustedVaultErrorSuggestionGenerator : public SuggestionGenerator {
 public:
  TrustedVaultErrorSuggestionGenerator();
  ~TrustedVaultErrorSuggestionGenerator() override;

  // SuggestionGenerator:
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      AutofillClient& client,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_TRUSTED_VAULT_TRUSTED_VAULT_ERROR_SUGGESTION_GENERATOR_H_
