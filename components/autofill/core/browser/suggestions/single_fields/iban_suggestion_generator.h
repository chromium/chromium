// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SINGLE_FIELDS_IBAN_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SINGLE_FIELDS_IBAN_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class IbanSuggestionGenerator : public SuggestionGenerator {
 public:
  void FetchSuggestionData(
      const FormStructure& form,
      const AutofillField& trigger_field,
      AutofillClient& client,
      base::OnceCallback<
          void(std::pair<FillingProduct,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormStructure& form,
      const AutofillField& trigger_field,
      AutofillClient& client,
      const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
          suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SINGLE_FIELDS_IBAN_SUGGESTION_GENERATOR_H_
