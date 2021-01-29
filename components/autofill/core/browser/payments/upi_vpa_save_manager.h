// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UPI_VPA_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UPI_VPA_SAVE_MANAGER_H_

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

// Manages the saving of UPI Virtual Payment Addresses. UPI is an online payment
// system. See https://en.wikipedia.org/wiki/Unified_Payments_Interface
class UpiVpaSaveManager {
 public:
  UpiVpaSaveManager(AutofillClient* client,
                    PersonalDataManager* personal_data_manager);
  ~UpiVpaSaveManager();

  // Offer to save |upi_id| locally on the user's profile.
  void OfferLocalSave(const std::string& upi_id);

 private:
  void OnUserDecidedOnLocalSave(const std::string& upi_id, bool accepted);

  AutofillClient* client_;

  // The personal data manager, used to save and load personal data to/from the
  // web database. This is overridden by the AutofillManagerTest.
  // Weak reference. May be nullptr, which indicates OTR.
  PersonalDataManager* personal_data_manager_;

  base::WeakPtrFactory<UpiVpaSaveManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UPI_VPA_SAVE_MANAGER_H_
