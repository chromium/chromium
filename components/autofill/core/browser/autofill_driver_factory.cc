// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_driver_factory.h"

#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"

namespace autofill {

AutofillDriverFactory::AutofillDriverFactory() = default;
AutofillDriverFactory::~AutofillDriverFactory() = default;

void AutofillDriverFactory::SetLifecycleStateAndNotifyObservers(
    AutofillDriver& driver,
    const LifecycleState new_state) {
  const LifecycleState old_state = driver.GetLifecycleState();
  if (old_state == new_state) {
    return;
  }
  driver.SetLifecycleState(new_state, /*pass_key=*/{});
  for (auto& observer : observers()) {
    observer.OnAutofillDriverStateChanged(*this, driver, old_state, new_state);
  }
  driver.GetAutofillManager().OnAutofillDriverLifecycleStateChanged(
      old_state, new_state, /*pass_key=*/{});
}

}  // namespace autofill
