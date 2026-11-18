// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_TEST_API_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_TEST_API_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/android_autofill/browser/form_field_data_android.h"

namespace autofill {

class FormFieldDataAndroidBridge;

class FormDataAndroidTestApi {
 public:
  explicit FormDataAndroidTestApi(FormDataAndroid* form) : form_(*form) {}

  const std::vector<std::unique_ptr<FormFieldDataAndroid>>& fields() && {
    return form_->fields_;
  }

  FormFieldDataAndroidBridge& field_bridge(size_t index) && {
    return *form_->fields_.at(index)->bridge_;
  }

 private:
  const raw_ref<FormDataAndroid> form_;
};

inline FormDataAndroidTestApi test_api(FormDataAndroid& form) {
  return FormDataAndroidTestApi(&form);
}

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_TEST_API_H_
