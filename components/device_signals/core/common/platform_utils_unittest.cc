// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_utils.h"

#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

class PlatformUtilsTest : public testing::Test {
 protected:
  void TearDown() override { internal::ClearMacAddressesForTesting(); }
};

TEST_F(PlatformUtilsTest,
       NormalizeMacAddresses_RemovesDuplicatesAndEmptyEntries) {
  std::vector<std::string> mac_addresses = {
      "00:1A:2B:3C:4D:5E", "", "FF:FF:FF:FF:FF:FF", "00:1A:2B:3C:4D:5E",
      "AB:CD:EF:12:34:56", " "};
  internal::SetMacAddressesForTesting(mac_addresses);
  auto result = GetMacAddresses();
  std::vector<std::string> expected = {"00:1A:2B:3C:4D:5E", "FF:FF:FF:FF:FF:FF",
                                       "AB:CD:EF:12:34:56"};
  EXPECT_THAT(result, testing::UnorderedElementsAreArray(expected));
}

TEST_F(PlatformUtilsTest,
       NormalizeMacAddresses_RemoveCaseSensitiveDuplicatesOnly) {
  std::vector<std::string> mac_addresses = {
      "ab:cd:ef:12:34:56", "ab:cd:ef:12:34:56", "AB:CD:EF:12:34:56"};
  internal::SetMacAddressesForTesting(mac_addresses);
  auto result = GetMacAddresses();
  std::vector<std::string> expected = {"ab:cd:ef:12:34:56",
                                       "AB:CD:EF:12:34:56"};
  EXPECT_THAT(result, testing::UnorderedElementsAreArray(expected));
}

TEST_F(PlatformUtilsTest, NormalizeMacAddresses_EmptyInput) {
  std::vector<std::string> mac_addresses;
  internal::SetMacAddressesForTesting(mac_addresses);
  auto result = GetMacAddresses();
  EXPECT_TRUE(result.empty());
}

}  // namespace device_signals
