// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/address_data_manager.h"

namespace autofill {

class AddressDataManagerTestApi {
 public:
  explicit AddressDataManagerTestApi(AddressDataManager& adm) : adm_(adm) {}

  // Used to automatically import addresses without a prompt.
  void set_auto_accept_address_imports(bool auto_accept) {
    adm_->auto_accept_address_imports_for_testing_ = auto_accept;
  }

 private:
   raw_ref<AddressDataManager> adm_;
};

inline AddressDataManagerTestApi test_api(AddressDataManager& adm) {
  return AddressDataManagerTestApi(adm);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_TEST_API_H_
