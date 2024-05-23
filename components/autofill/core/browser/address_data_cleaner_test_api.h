// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_CLEANER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_CLEANER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/address_data_cleaner.h"

namespace autofill {

class AddressDataCleanerTestApi {
 public:
  explicit AddressDataCleanerTestApi(AddressDataCleaner& data_cleaner)
      : data_cleaner_(data_cleaner) {}

  void ApplyDeduplicationRoutine() {
    data_cleaner_->ApplyDeduplicationRoutine();
  }

  void DeleteDisusedAddresses() { data_cleaner_->DeleteDisusedAddresses(); }

  bool AreCleanupsPending() const {
    return data_cleaner_->are_cleanups_pending_;
  }

  void ResetAreCleanupsPending() {
    data_cleaner_->are_cleanups_pending_ = true;
  }

 private:
  const raw_ref<AddressDataCleaner> data_cleaner_;
};

inline AddressDataCleanerTestApi test_api(AddressDataCleaner& data_cleaner) {
  return AddressDataCleanerTestApi(data_cleaner);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_CLEANER_TEST_API_H_
