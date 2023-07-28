// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "testing/gtest/include/gtest/gtest.h"

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
}

}  // namespace plus_addresses
