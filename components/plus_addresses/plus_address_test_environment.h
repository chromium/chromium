// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_ENVIRONMENT_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_ENVIRONMENT_H_

#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace plus_addresses::test {

// A helper class to provide test environments needed to instantiate a
// `PlusAddressService`.
class PlusAddressTestEnvironment final {
 public:
  PlusAddressTestEnvironment();
  PlusAddressTestEnvironment(const PlusAddressTestEnvironment&) = delete;
  PlusAddressTestEnvironment& operator=(const PlusAddressTestEnvironment&) =
      delete;
  PlusAddressTestEnvironment(PlusAddressTestEnvironment&&) = delete;
  PlusAddressTestEnvironment& operator=(PlusAddressTestEnvironment&&) = delete;
  ~PlusAddressTestEnvironment();

  affiliations::MockAffiliationService& affiliation_service() {
    return mock_affiliation_service_;
  }
  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  PrefService& pref_service() { return pref_service_; }
  FakePlusAddressSettingService& setting_service() { return setting_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  FakePlusAddressSettingService setting_service_;
  testing::NiceMock<affiliations::MockAffiliationService>
      mock_affiliation_service_;
};

}  // namespace plus_addresses::test

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_ENVIRONMENT_H_
