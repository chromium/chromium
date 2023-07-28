// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "base/test/scoped_feature_list.h"
#include "components/plus_addresses/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace plus_addresses {

class PlusAddressServiceTest : public ::testing::Test {};

TEST_F(PlusAddressServiceTest, BasicTest) {
  const std::string test_facet = "asdf";
  const std::string test_address = "mattwashere";
  PlusAddressService service;
  EXPECT_FALSE(service.IsPlusAddress(test_address));
  PreAllocatedPlusAddress pre_allocated_plus_address;
  pre_allocated_plus_address.address = test_address;
  service.SavePlusAddress(test_facet, pre_allocated_plus_address);
  EXPECT_TRUE(service.IsPlusAddress(test_address));
  EXPECT_EQ(service.GetPlusAddress(test_facet), test_address);
  EXPECT_EQ(service.GetPlusAddress("notanactualfacet"), absl::nullopt);
}

TEST_F(PlusAddressServiceTest, DefaultSupportsPlusAddressesState) {
  // Without explicit enablement of the feature, the `SupportsPlusAddresses`
  // function should return `false`.
  PlusAddressService service;
  EXPECT_FALSE(service.SupportsPlusAddresses());
}

TEST_F(PlusAddressServiceTest, FeatureEnabled) {
  // With explicit enablement of the feature, the `SupportsPlusAddresses`
  // function should return `true`.
  base::test::ScopedFeatureList scoped_feature_list;
  // With the flag set, the URL should be filtered.
  scoped_feature_list.InitAndEnableFeature(plus_addresses::kFeature);
  PlusAddressService service;
  EXPECT_TRUE(service.SupportsPlusAddresses());
}

TEST_F(PlusAddressServiceTest, FeatureExplicitlyDisabled) {
  // With explicit disabling of the feature, the `SupportsPlusAddresses`
  // function should return `false`.
  base::test::ScopedFeatureList scoped_feature_list;
  // With the flag set, the URL should be filtered.
  scoped_feature_list.InitAndDisableFeature(plus_addresses::kFeature);
  PlusAddressService service;
  EXPECT_FALSE(service.SupportsPlusAddresses());
}

}  // namespace plus_addresses
