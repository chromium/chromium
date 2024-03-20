// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace plus_addresses::test {

// Returns a fully populated, confirmed PlusProfile.
PlusProfile GetPlusProfile();
// Returns a fully populated, confirmed PlusProfile different from
// `GetPlusProfile()`.
PlusProfile GetPlusProfile2();

// Used in testing the GetOrCreate, Reserve, and Create network requests.
std::string MakeCreationResponse(const PlusProfile& profile);
// Used in testing the List network requests.
std::string MakeListResponse(const std::vector<PlusProfile>& profiles);
// Converts a PlusProfile to an equivalent JSON string.
std::string MakePlusProfile(const PlusProfile& profile);

// Waits for the next `PlusAddressService::Observer::OnPlusAddressesChanged()`
// event is triggered.
class PlusAddressesChangedWaiter {
 public:
  explicit PlusAddressesChangedWaiter(PlusAddressService* service);
  ~PlusAddressesChangedWaiter();

  void Wait() &&;

 private:
  class MockObserver : public PlusAddressService::Observer {
   public:
    MockObserver();
    ~MockObserver() override;
    MOCK_METHOD(void, OnPlusAddressesChanged, (), (override));
  };
  base::RunLoop run_loop_;
  testing::NiceMock<MockObserver> mock_observer_;
  base::ScopedObservation<PlusAddressService, MockObserver> scoped_observation_{
      &mock_observer_};
};

}  // namespace plus_addresses::test

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
