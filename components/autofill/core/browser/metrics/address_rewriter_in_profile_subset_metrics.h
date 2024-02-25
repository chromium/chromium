// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_REWRITER_IN_PROFILE_SUBSET_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_REWRITER_IN_PROFILE_SUBSET_METRICS_H_

#include <stddef.h>

namespace autofill::autofill_metrics {

// Records the number of suggestions that were hidden prior to the effects of
// the feature kAutofillUseAddressRewriterInProfileSubsetComparison. Emitted
// once per suggestion generation.
void LogPreviouslyHiddenProfileSuggestionNumber(size_t hidden_profiles_number);

// Records whether the user accepted a suggestion that was previously hidden
// prior to the effects caused by the feature
// kAutofillUseAddressRewriterInProfileSubsetComparison. Emitted every time the
// user accepts a `PopupItemId::kAddressEntry` suggestion.
void LogUserAcceptedPreviouslyHiddenProfileSuggestion(bool previously_hidden);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_REWRITER_IN_PROFILE_SUBSET_METRICS_H_
