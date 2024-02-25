// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_TEST_API_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/android_autofill/browser/form_data_android.h"

namespace autofill {

class FormDataAndroidTestApi {
 public:
  explicit FormDataAndroidTestApi(FormDataAndroid* form) : form_(*form) {}

  const std::vector<std::unique_ptr<FormFieldDataAndroid>>& fields() && {
    return form_->fields_;
  }

 private:
  const raw_ref<FormDataAndroid> form_;
};

inline FormDataAndroidTestApi test_api(FormDataAndroid& form) {
  return FormDataAndroidTestApi(&form);
}

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_TEST_API_H_
