// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_ADDRESS_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_ADDRESS_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/addresses/address.h"

namespace autofill {

class AddressComponent;

class AddressTestApi {
 public:
  explicit AddressTestApi(Address& address) : address_(address) {}

  AddressComponent* Root() { return address_->Root(); }

 private:
  raw_ref<Address> address_;
};

inline AddressTestApi test_api(Address& address) {
  return AddressTestApi(address);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_ADDRESS_TEST_API_H_
