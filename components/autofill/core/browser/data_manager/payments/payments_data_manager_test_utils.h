// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"

namespace autofill {

// Helper class to wait for an `OnPaymentsDataChanged()` call from the `paydm`.
// This is necessary since PayDM operates asynchronously on the WebDatabase.
// Example usage:
//   paydm.AddCreditCard(CreditCard());
//   PaymentsDataManagerWaiter(&paydm).Wait();
class PaymentsDataChangedWaiter : public PaymentsDataManager::Observer {
 public:
  explicit PaymentsDataChangedWaiter(PaymentsDataManager* paydm);
  ~PaymentsDataChangedWaiter() override;

  // Waits for `OnPaymentsDataChanged()` to trigger.
  void Wait(const base::Location& location = FROM_HERE) &&;

  // PaymentsDataManager::Observer:
  void OnPaymentsDataChanged() override;

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<PaymentsDataManager, PaymentsDataManager::Observer>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_TEST_UTILS_H_
