// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_driver_factory.h"

namespace autofill {

// Exposes some testing operations for AutofillDriverFactory.
class AutofillDriverFactoryTestApi {
 public:
  explicit AutofillDriverFactoryTestApi(AutofillDriverFactory* factory);

  void SetLifecycleStateAndNotifyObservers(
      AutofillDriver& driver,
      AutofillDriver::LifecycleState new_state) {
    factory_->SetLifecycleStateAndNotifyObservers(driver, new_state);
  }

  // Like the normal AddObserver(), but enqueues `observer` at position `index`
  // in the list, so that `observer` is notified before production-code
  // observers.
  void AddObserverAtIndex(AutofillDriverFactory::Observer* observer,
                          size_t index);

 protected:
  const raw_ref<AutofillDriverFactory> factory_;
};

inline AutofillDriverFactoryTestApi test_api(AutofillDriverFactory& factory) {
  return AutofillDriverFactoryTestApi(&factory);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
