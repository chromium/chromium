// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/single_fields/iban_suggestion_generator.h"

#include "base/barrier_callback.h"
#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/payments/iban_manager.h"

namespace autofill {

void IbanSuggestionGenerator::FetchSuggestionData(
    const FormStructure& form,
    const AutofillField& trigger_field,
    AutofillClient& client,
    base::OnceCallback<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  std::move(callback).Run(
      {FillingProduct::kIban,
       base::ToVector(
           client.GetPaymentsAutofillClient()
               ->GetPaymentsDataManager()
               .GetOrderedIbansToSuggest(),
           [](Iban& iban) { return SuggestionData(std::move(iban)); })});
}

void IbanSuggestionGenerator::GenerateSuggestions(
    const FormStructure& form,
    const AutofillField& trigger_field,
    AutofillClient& client,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  IbanManager* iban_manager =
      client.GetPaymentsAutofillClient()->GetIbanManager();
  if (!iban_manager ||
      GetSuggestionDataForFillingProduct(suggestion_data, FillingProduct::kIban)
          .empty()) {
    // There are no IBAN suggestions to return.
    std::move(callback).Run({FillingProduct::kIban, {}});
    return;
  }

  auto wrapped_callback = base::BarrierCallback<std::vector<Suggestion>>(
      1, base::BindOnce(
             [](base::OnceCallback<void(ReturnedSuggestions)> callback,
                std::vector<std::vector<Suggestion>> suggestions) {
               std::move(callback).Run({FillingProduct::kIban, suggestions[0]});
             },
             std::move(callback)));
  // TODO(crbug.com/409962888): Inline in `OnGetSingleFieldSuggestions` call.
  auto on_suggestions_returned = base::BindOnce(
      [](base::OnceCallback<void(std::vector<Suggestion>)> callback,
         FieldGlobalId field_id, const std::vector<Suggestion>& suggestions) {
        std::move(callback).Run(suggestions);
      },
      wrapped_callback);

  // TODO(crbug.com/409962888): Move the suggestion generation logic from the
  // `IbanManager` to this class.
  // TODO(crbug.com/409962888): `OnGetSingleFieldSuggestions` should use the
  // suggestion data instead of calling the `PaymentsDataManager` again.
  if (!iban_manager->OnGetSingleFieldSuggestions(
          trigger_field, trigger_field, client, on_suggestions_returned)) {
    wrapped_callback.Run({});
  }
}
}  // namespace autofill
