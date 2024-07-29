// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_driver.h"

namespace autofill {

// Exposes some testing operations for AutofillDriver.
class AutofillDriverTestApi {
 public:
  explicit AutofillDriverTestApi(AutofillDriver* driver) : driver_(*driver) {}

  void SetLifecycleState(AutofillDriver::LifecycleState lifecycle_state) {
    driver_->SetLifecycleState(lifecycle_state, []() {}, {});
  }

  void SetLifecycleStateWithoutNotifications(
      AutofillDriver::LifecycleState lifecycle_state) {
    driver_->lifecycle_state_ = lifecycle_state;
  }

 private:
  raw_ref<AutofillDriver> driver_;
};

inline AutofillDriverTestApi test_api(AutofillDriver& driver) {
  return AutofillDriverTestApi(&driver);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_TEST_API_H_
