// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UPI_VPA_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UPI_VPA_SAVE_MANAGER_H_

#include "base/strings/string16.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

class UpiVpaSaveManager {
 public:
  UpiVpaSaveManager(PersonalDataManager* personal_data_manager);
  ~UpiVpaSaveManager() = default;

  void OfferLocalSave(const std::string& upi_id);

 private:
  // The personal data manager, used to save and load personal data to/from the
  // web database. This is overridden by the AutofillManagerTest.
  // Weak reference. May be nullptr, which indicates OTR.
  PersonalDataManager* personal_data_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UPI_VPA_SAVE_MANAGER_H_
