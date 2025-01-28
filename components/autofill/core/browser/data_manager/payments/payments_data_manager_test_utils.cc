// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_utils.h"

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/scoped_run_loop_timeout.h"

namespace autofill {

PaymentsDataChangedWaiter::PaymentsDataChangedWaiter(
    PaymentsDataManager* paydm) {
  scoped_observation_.Observe(paydm);
}

PaymentsDataChangedWaiter::~PaymentsDataChangedWaiter() = default;

void PaymentsDataChangedWaiter::Wait(const base::Location& location) && {
  // Log the location from whence `Wait` was called in case of timeout.
  base::test::ScopedRunLoopTimeout timeout(location, std::nullopt,
                                           base::NullCallback());
  run_loop_.Run();
}

void PaymentsDataChangedWaiter::OnPaymentsDataChanged() {
  run_loop_.Quit();
}

}  // namespace autofill
