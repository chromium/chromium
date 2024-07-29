// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_driver.h"

#include "base/check.h"
#include "base/functional/function_ref.h"
#include "components/autofill/core/browser/autofill_manager.h"

namespace autofill {

AutofillDriver::~AutofillDriver() {
  CHECK_EQ(lifecycle_state_, LifecycleState::kPendingDeletion,
           base::NotFatalUntil::M130);
}

void AutofillDriver::SetLifecycleState(
    const LifecycleState new_state,
    base::FunctionRef<void()> notify_observers_callback,
    AutofillDriverFactoryPassKey) {
  const LifecycleState old_state = lifecycle_state_;
  if (old_state == new_state) {
    return;
  }
  lifecycle_state_ = new_state;
  notify_observers_callback();
  GetAutofillManager().OnAutofillDriverLifecycleStateChanged(old_state,
                                                             new_state, {});
}

}  // namespace autofill
