// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_IDENTITY_CREDENTIAL_SUGGESTION_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_IDENTITY_CREDENTIAL_SUGGESTION_UTILS_H_

#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

// Wraps the `GetVerifiedAutofillSuggestions` method of the
// `IdentityCredentialDelegate`. It is used by password manager as `FormData`
// and `FormFieldData` are required by the `SuggestionGenerator` API, but not
// easily obtainable there.
// TODO(crbug.com/409962888): Remove once the password manager's suggestion
// generation is migrated to the new architecture.
std::vector<Suggestion> GetIdentityCredentialSuggestionsForType(
    const IdentityCredentialDelegate* delegate,
    const AutofillClient& autofill_client,
    const FieldType& field_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_IDENTITY_CREDENTIAL_SUGGESTION_UTILS_H_
