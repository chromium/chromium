// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_TEST_API_H_

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/safe_conversions.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

// Exposes some testing (and debugging) operations for FormData.
class FormDataTestApi {
 public:
  explicit FormDataTestApi(FormData* form) : form_(*form) {}

  std::vector<FormFieldData>& fields() { return form_->fields_; }

  FormFieldData& field(int i) { return form_->fields_[to_index(i)]; }

  void Resize(size_t n) { form_->fields_.resize(n); }

  void Append(FormFieldData field) {
    form_->fields_.push_back(std::move(field));
  }
  void Append(base::span<const FormFieldData> fields) {
    Insert(form_->fields_.size(), fields);
  }

  void Insert(int i, FormFieldData field) {
    form_->fields_.insert(form_->fields_.begin() + to_index(i),
                          std::move(field));
  }
  void Insert(int i, base::span<const FormFieldData> fields) {
    form_->fields_.insert(form_->fields_.begin() + to_index(i), fields.begin(),
                          fields.end());
  }

  void Remove(int i) {
    form_->fields_.erase(form_->fields_.begin() + to_index(i));
  }

 private:
  size_t to_index(int i) {
    if (i < 0) {
      i = form_->fields_.size() + i;
    }
    CHECK(0 <= i && i <= base::checked_cast<int>(form_->fields_.size()));
    return i;
  }

  const raw_ref<FormData> form_;
};

inline FormDataTestApi test_api(FormData& form) {
  return FormDataTestApi(&form);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_TEST_API_H_
