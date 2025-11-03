// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_TEST_API_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

// Exposes some testing operations for AutofillField.
class AutofillFieldTestApi {
 public:
  explicit AutofillFieldTestApi(AutofillField& autofill_field)
      : autofill_field_(autofill_field) {}

  void set_initial_value(std::u16string initial_value) {
    autofill_field_->initial_value_ = std::move(initial_value);
  }

 private:
  const raw_ref<AutofillField> autofill_field_;
};

inline AutofillFieldTestApi test_api(AutofillField& autofill_field) {
  return AutofillFieldTestApi(autofill_field);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_TEST_API_H_
