// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_DATA_CLEANER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_DATA_CLEANER_H_

#include "base/memory/raw_ptr.h"

namespace autofill {

class PaymentsDataManager;

// PaymentsDataCleaner is responsible for applying credit card cleanups once on
// browser startup.
class PaymentsDataCleaner {
 public:
  explicit PaymentsDataCleaner(PaymentsDataManager* payments_data_manager);
  ~PaymentsDataCleaner();
  PaymentsDataCleaner(const PaymentsDataCleaner&) = delete;
  PaymentsDataCleaner& operator=(const PaymentsDataCleaner&) = delete;

  // Applies payments cleanups.
  void CleanupPaymentsData();

 private:
  friend class PaymentsDataCleanerTest;

  // Tries to delete disused credit cards on startup.
  bool DeleteDisusedCreditCards();

  // Clears the value of the origin field of cards that were not created from
  // the settings page.
  void ClearCreditCardNonSettingsOrigins();

  // The payments data manager, used to load and update the payments data
  // from/to the web database.
  const raw_ptr<PaymentsDataManager> payments_data_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_DATA_CLEANER_H_
