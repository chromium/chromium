// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"

namespace autofill {

// A simplistic PersonalDataManager used for testing. It doesn't interact with
// AutofillTable. Instead, it keeps all data in memory only. It is thus fully
// synchronous.
class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager();

  TestPersonalDataManager(const TestPersonalDataManager&) = delete;
  TestPersonalDataManager& operator=(const TestPersonalDataManager&) = delete;

  ~TestPersonalDataManager() override;

  TestAddressDataManager& test_address_data_manager() {
    AddressDataManager& manager = address_data_manager();
    return static_cast<TestAddressDataManager&>(manager);
  }
  const TestAddressDataManager& test_address_data_manager() const {
    const AddressDataManager& manager = address_data_manager();
    return static_cast<const TestAddressDataManager&>(manager);
  }
  TestPaymentsDataManager& test_payments_data_manager() {
    PaymentsDataManager& manager = payments_data_manager();
    return static_cast<TestPaymentsDataManager&>(manager);
  }
  const TestPaymentsDataManager& test_payments_data_manager() const {
    const PaymentsDataManager& manager = payments_data_manager();
    return static_cast<const TestPaymentsDataManager&>(manager);
  }

  // Can be used to inject mock instances.
  void set_address_data_manager(
      std::unique_ptr<TestAddressDataManager> address_data_manager);
  void set_payments_data_manager(
      std::unique_ptr<TestPaymentsDataManager> payments_data_manager);

  // PersonalDataManager overrides.  These functions are overridden as needed
  // for various tests, whether to skip calls to uncreated databases/services,
  // or to make things easier in general to toggle.
  bool IsDataLoaded() const override;

  // Unique to TestPersonalDataManager:
  void SetPrefService(PrefService* pref_service);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
