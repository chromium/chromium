// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class PlusAddressSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit PlusAddressSuggestionGenerator(
      AutofillClient* client,
      PasswordFormClassification password_form_classification,
      bool is_manually_triggered);

  ~PlusAddressSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource, std::vector<SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

 private:
  AutofillPlusAddressDelegate* plus_address_delegate() const {
    return client_->GetPlusAddressDelegate();
  }

  const raw_ptr<AutofillClient> client_;

  PasswordFormClassification password_form_classification_;

  // TODO(crbug.com/409962888): remove [[maybe_unused]] when logic is added
  [[maybe_unused]] bool is_manually_triggered_;

  base::WeakPtrFactory<PlusAddressSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_
