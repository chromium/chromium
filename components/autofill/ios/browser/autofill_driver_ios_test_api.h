// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_TEST_API_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_TEST_API_H_

#include "components/autofill/core/browser/autofill_driver_test_api.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"

namespace autofill {

// Exposes some testing operations for AutofillDriverIOS.
class AutofillDriverIOSTestApi : public AutofillDriverTestApi {
 public:
  explicit AutofillDriverIOSTestApi(AutofillDriverIOS* driver)
      : AutofillDriverTestApi(driver) {}

  void SetAutofillManager(std::unique_ptr<BrowserAutofillManager> manager) {
    driver().manager_observation_.Reset();
    driver().manager_ = std::move(manager);
    driver().manager_observation_.Observe(driver().manager_.get());
  }

 private:
  AutofillDriverIOS& driver() {
    return static_cast<AutofillDriverIOS&>(*driver_);
  }
};

inline AutofillDriverIOSTestApi test_api(AutofillDriverIOS& driver) {
  return AutofillDriverIOSTestApi(&driver);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_TEST_API_H_
