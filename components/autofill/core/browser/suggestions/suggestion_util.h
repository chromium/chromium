// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_
namespace autofill {

class AutofillField;

// Returns true if suggestions should be suppressed on `field` because of it
// having an unrecognized HTML autocomplete attribute.
bool SuppressSuggestionsForAutocompleteUnrecognizedField(
    const AutofillField& field);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_
