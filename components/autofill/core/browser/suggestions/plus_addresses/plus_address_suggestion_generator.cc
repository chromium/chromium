// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {

PlusAddressSuggestionGenerator::PlusAddressSuggestionGenerator(
    AutofillClient* client,
    PasswordFormClassification password_form_classification,
    bool is_manually_triggered)
    : client_(client),
      password_form_classification_(password_form_classification),
      is_manually_triggered_(is_manually_triggered) {}

PlusAddressSuggestionGenerator::~PlusAddressSuggestionGenerator() = default;

void PlusAddressSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<void(std::pair<SuggestionDataSource,
                                      std::vector<SuggestionData>>)> callback) {
}

void PlusAddressSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {}

}  // namespace autofill
