// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

// Exposes some testing (and debugging) operations for FormData.
class FormDataTestApi {
 public:
  explicit FormDataTestApi(FormData* form) : form_(*form) {}

  // TODO: crbug.com/40232021 - Introduce and use shorthands bulky expressions:
  // - `field(i)` for `fields()[i]`
  // - `Push()` for `fields().push_back()`
  // - `Pop()` for `fields().pop_back()`
  // - `Erase(i)` for `fields().erase(fields().begin() + i)`
  std::vector<FormFieldData>& fields() { return form_->fields; }

 private:
  const raw_ref<FormData> form_;
};

inline FormDataTestApi test_api(FormData& form) {
  return FormDataTestApi(&form);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_TEST_API_H_
