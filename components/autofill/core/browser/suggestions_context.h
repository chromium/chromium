// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_CONTEXT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_CONTEXT_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

// Indicates the reason why autofill suggestions are suppressed.
enum class SuppressReason {
  kNotSuppressed,
  // Suggestions are not shown because an ablation experiment is enabled.
  kAblation,
  // Address suggestions are not shown because the field is annotated with
  // autocomplete=off and the directive is being observed by the browser.
  kAutocompleteOff,
  // Suggestions are not shown because this form is on a secure site, but
  // submits insecurely. This is only used when the user has started typing,
  // otherwise a warning is shown.
  kInsecureForm,
  // Suggestions are not shown because the field is annotated with
  // an unrecognized autocompelte attribute and the field is not credit card
  // related. For credit card fields, the unrecognized attribute is ignored.
  kAutocompleteUnrecognized,
};

// The context for the list of suggestions available for a given field.
struct SuggestionsContext {
  SuggestionsContext();
  ~SuggestionsContext();

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION FormStructure* form_structure = nullptr;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION AutofillField* focused_field = nullptr;
  bool is_autofill_available = false;
  bool is_context_secure = false;
  bool is_filling_credit_card = false;
  // Flag to indicate whether all suggestions come from Google Payments.
  bool should_display_gpay_logo = false;
  SuppressReason suppress_reason = SuppressReason::kNotSuppressed;
  // Indicates whether the form filling is under ablation, meaning that
  // autofill popups are suppressed.
  AblationGroup ablation_group = AblationGroup::kDefault;
  // Indicates whether the form filling is under ablation, under the condition
  // that the user has data to fill on file. All users that don't have data
  // to fill are in the AbationGroup::kDefault.
  // Note that it is possible (due to implementation details) that this is
  // incorrectly set to kDefault: If the user has typed some characters into a
  // text field, it may look like no suggestions are available, but in
  // practice the suggestions are just filtered out (Autofill only suggests
  // matches that start with the typed prefix). Any consumers of the
  // conditional_ablation_group attribute should monitor it over time.
  // Any transitions of conditional_ablation_group from {kAblation,
  // kControl} to kDefault should just be ignored and the previously reported
  // value should be used. As the ablation experience is stable within a day,
  // such a transition typically indicates that the user has type a prefix
  // which led to the filtering of all autofillable data. In short: once
  // either kAblation or kControl were reported, consumers should stick to
  // that.
  AblationGroup conditional_ablation_group = AblationGroup::kDefault;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_CONTEXT_H_
