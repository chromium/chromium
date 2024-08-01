// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_driver_test_api.h"

namespace autofill {

// Exposes some testing operations for ContentAutofillDriver.
class ContentAutofillDriverTestApi : public AutofillDriverTestApi {
 public:
  explicit ContentAutofillDriverTestApi(ContentAutofillDriver* driver)
      : AutofillDriverTestApi(driver) {}

  void set_autofill_manager(std::unique_ptr<AutofillManager> autofill_manager) {
    driver().autofill_manager_ = std::move(autofill_manager);
  }

  void LiftForTest(FormData& form) { driver().LiftForTest(form); }

 private:
  ContentAutofillDriver& driver() {
    return static_cast<ContentAutofillDriver&>(*driver_);
  }
};

inline ContentAutofillDriverTestApi test_api(ContentAutofillDriver& driver) {
  return ContentAutofillDriverTestApi(&driver);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_
