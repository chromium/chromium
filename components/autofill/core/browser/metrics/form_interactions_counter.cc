// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_interactions_counter.h"

namespace autofill {

FormInteractionsCounter::~FormInteractionsCounter() = default;

void FormInteractionsCounter::OnTextFieldDidChange(
    const FieldSignature& field_signature) {
  if (field_signature != last_field_signature_modified_by_user_) {
    form_interaction_counts_.form_element_user_modifications++;
    last_field_signature_modified_by_user_ = field_signature;
  }
}

void FormInteractionsCounter::OnAutofillFill() {
  form_interaction_counts_.autofill_fills++;
}

void FormInteractionsCounter::OnAutocompleteFill() {
  form_interaction_counts_.autocomplete_fills++;
}

const FormInteractionCounts& FormInteractionsCounter::GetCounts() const {
  return form_interaction_counts_;
}

}  // namespace autofill
