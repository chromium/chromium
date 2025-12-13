// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/identity_credential_suggestion_utils.h"

#include "components/autofill/core/common/form_data.h"

namespace autofill {

std::vector<Suggestion> GetIdentityCredentialSuggestionsForType(
    const IdentityCredentialDelegate* delegate,
    const AutofillClient& autofill_client,
    const FieldType& field_type) {
  if (!delegate) {
    return {};
  }
  // Given that the `identity_credential_delegate` only uses the autofill field
  // type to check whether a suggestion should be shown, the `form` and `field`
  // created here are empty apart from the field type.
  autofill::FormData form;
  autofill::FormFieldData field;
  autofill::AutofillField autofill_field(field);
  autofill_field.set_heuristic_type(autofill::HeuristicSource::kRegexes,
                                    field_type);
  return delegate->GetVerifiedAutofillSuggestions(
      form, /*form_structure=*/nullptr, field, &autofill_field,
      autofill_client);
}

}  // namespace autofill
