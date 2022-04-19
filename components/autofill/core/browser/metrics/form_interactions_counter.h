// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_INTERACTIONS_COUNTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_INTERACTIONS_COUNTER_H_

#include "components/autofill/core/common/signatures.h"

namespace autofill {

struct FormInteractionCounts {
  int64_t form_element_user_modifications = 0;
  int64_t autofill_fills = 0;
  int64_t autocomplete_fills = 0;
};

// Holds and increments counts of user interactions with autofillable form
// elements, Autofill and Autocomplete
class FormInteractionsCounter {
 public:
  FormInteractionsCounter() = default;
  FormInteractionsCounter(const FormInteractionsCounter&) = delete;
  FormInteractionsCounter& operator=(const FormInteractionsCounter&) = delete;

  virtual ~FormInteractionsCounter();

  void OnTextFieldDidChange(const FieldSignature& field_signature);

  void OnAutofillFill();

  void OnAutocompleteFill();

  const FormInteractionCounts& GetCounts() const;

 private:
  FieldSignature last_field_signature_modified_by_user_;

  FormInteractionCounts form_interaction_counts_ = {};
};

}  // namespace autofill

#endif
