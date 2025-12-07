// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTIONS_CONTEXT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTIONS_CONTEXT_H_

#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/studies/autofill_ablation_study.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

// Indicates the reason why autofill suggestions are suppressed.
enum class SuppressReason {
  kNotSuppressed,
  // Suggestions are not shown because an ablation experiment is enabled.
  kAblation,
  // Suggestions are not shown because the field is annotated with
  // an unrecognized autocomplete attribute and the field is not credit card
  // related. For credit card fields, the unrecognized attribute is ignored.
  kAutocompleteUnrecognized,
};

// The context for the list of suggestions available for a given field.
struct SuggestionsContext {
  SuggestionsContext();
  SuggestionsContext(const SuggestionsContext&);
  SuggestionsContext& operator=(const SuggestionsContext&);
  ~SuggestionsContext();

  FillingProduct filling_product = FillingProduct::kNone;
  SuppressReason suppress_reason = SuppressReason::kNotSuppressed;
  // Indicates whether generating Autofill and AutofillAI suggestions
  // should be avoided. This can happen in multiple scenarios (e.g. during
  // manual fallbacks for plus addresses, due to unrecognized autocomplete
  // attribute, or if the form is a mixed content form).
  // TODO(crbug.com/409962888): Remove once each suggestion generator is capable
  // of checking all of their requirements.
  bool do_not_generate_autofill_suggestions = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTIONS_CONTEXT_H_
