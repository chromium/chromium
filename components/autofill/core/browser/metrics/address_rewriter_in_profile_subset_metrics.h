// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_REWRITER_IN_PROFILE_SUBSET_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_REWRITER_IN_PROFILE_SUBSET_METRICS_H_

namespace autofill::autofill_metrics {

// Records whether a profile, considered a quasi-subset of another profile,
// which is a subset of another profile up to street address types, has a
// different street address or not. This is logged in
// AutofillProfile::IsSubsetOfForFieldSet, only when the field set contains a
// street address type, and is used to asses the feature
// `kAutofillUseAddressRewriterInProfileSubsetComparison`.
void LogProfilesDifferOnAddressLineOnly(bool has_different_address);

// Records whether the user accepted a suggestion that was previously hidden, or
// chose the suggestion that was responsible for other ones being hidden.
void LogUserAcceptedPreviouslyHiddenProfileSuggestion();

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_REWRITER_IN_PROFILE_SUBSET_METRICS_H_
