// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/content/browser/content_autofill_driver.h"

namespace autofill {

// Exposes some testing operations for ContentAutofillDriver.
class ContentAutofillDriverTestApi {
 public:
  explicit ContentAutofillDriverTestApi(ContentAutofillDriver* driver)
      : driver_(*driver) {}

  void SetFrameAndFormMetaData(FormData& form, FormFieldData* field) const {
    driver_->SetFrameAndFormMetaData(form, field);
  }

  FormData GetFormWithFrameAndFormMetaData(const FormData& form) const {
    return driver_->GetFormWithFrameAndFormMetaData(form);
  }

 private:
  const raw_ref<ContentAutofillDriver> driver_;
};

inline ContentAutofillDriverTestApi test_api(ContentAutofillDriver& driver) {
  return ContentAutofillDriverTestApi(&driver);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_
