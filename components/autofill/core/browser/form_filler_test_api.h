// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FILLER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FILLER_TEST_API_H_

#include "components/autofill/core/browser/form_filler.h"

namespace autofill {

// Exposes some testing operations for BrowserAutofillManager.
class FormFillerTestApi {
 public:
  explicit FormFillerTestApi(FormFiller* form_filler)
      : form_filler_(*form_filler) {}

  void set_limit_before_refill(base::TimeDelta limit) {
    form_filler_->limit_before_refill_ = limit;
  }

  void AddFormFillEntry(
      base::span<const FormFieldData* const> filled_fields,
      base::span<const AutofillField* const> filled_autofill_fields,
      FillingProduct filling_product,
      bool is_refill) {
    form_filler_->form_autofill_history_.AddFormFillEntry(
        filled_fields, filled_autofill_fields, filling_product, is_refill);
  }

 private:
  raw_ref<FormFiller> form_filler_;
};

inline FormFillerTestApi test_api(FormFiller& form_filler) {
  return FormFillerTestApi(&form_filler);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FILLER_TEST_API_H_
