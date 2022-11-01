// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/public/cpp/account_key_filter.h"

#include <cstdint>
#include <iterator>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

const std::vector<uint8_t> kAccountKey1{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};
const std::vector<uint8_t> kFilterBytes1{0x0A, 0x42, 0x88, 0x10};

const std::vector<uint8_t> kAccountKey2{0x11, 0x11, 0x22, 0x22, 0x33, 0x33,
                                        0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
                                        0x77, 0x77, 0x88, 0x88};
const std::vector<uint8_t> kFilterBytes1And2{0x2F, 0xBA, 0x06, 0x42, 0x00};

const std::vector<uint8_t> kFilterWithBattery{0x4A, 0x00, 0xF0, 0x00};
const std::vector<uint8_t> kBatteryData{0b00110011, 0b01000000, 0b01000000,
                                        0b01000000};
const uint8_t salt = 0xC7;

const std::vector<uint8_t> kAccountKey3{0x04, 0x70, 0xBA, 0xF8, 0x73, 0x19,
                                        0xCE, 0xF3, 0xA7, 0x97, 0x78, 0x52,
                                        0xB3, 0x4F, 0x4C, 0xD8};
const std::vector<uint8_t> kFilter3{0xB8, 0x2A, 0x41, 0xE2, 0x21};
const std::vector<uint8_t> kSalt3{0x6C, 0xE5};
const std::vector<uint8_t> kBatteryData3{0x33, 0xE4, 0xE4, 0x4C};

// Values for SASS enabled Pixel Buds Pro that failed to subsequent pair prior
// to ccrev.com/3953062 landing.
// Value source: b/243855406#comment24.
const std::vector<uint8_t> kAccountKey4{0x04, 0x3F, 0xC1, 0x8C, 0x63, 0xDC,
                                        0x75, 0x1A, 0xE8, 0x1A, 0xCF, 0x65,
                                        0x10, 0x15, 0x1D, 0xB0};
const std::vector<uint8_t> kFilter4{0x19, 0x23, 0x50, 0xE8, 0x37,
                                    0x68, 0xF0, 0x65, 0x22};
const std::vector<uint8_t> kSalt4{0xD7, 0xDE};
const std::vector<uint8_t> kBatteryData4{0x33, 0xE4, 0xE4, 0x64};

class AccountKeyFilterTest : public testing::Test {};

TEST_F(AccountKeyFilterTest, EmptyFilter) {
  AccountKeyFilter filter({}, {});
  EXPECT_FALSE(filter.IsAccountKeyInFilter(kAccountKey1));
  EXPECT_FALSE(filter.IsAccountKeyInFilter(kAccountKey2));
}

TEST_F(AccountKeyFilterTest, EmptyVectorTest) {
  EXPECT_FALSE(
      AccountKeyFilter(kFilterBytes1, {salt}).IsAccountKeyInFilter(std::vector<uint8_t>(0)));
}

TEST_F(AccountKeyFilterTest, SingleAccountKey_AsBytes) {
  EXPECT_TRUE(AccountKeyFilter(kFilterBytes1, {salt}).IsAccountKeyInFilter(kAccountKey1));
}

TEST_F(AccountKeyFilterTest, TwoAccountKeys_AsBytes) {
  AccountKeyFilter filter(kFilterBytes1And2, {salt});

  EXPECT_TRUE(filter.IsAccountKeyInFilter(kAccountKey1));
  EXPECT_TRUE(filter.IsAccountKeyInFilter(kAccountKey2));
}

TEST_F(AccountKeyFilterTest, MissingAccountKey) {
  const std::vector<uint8_t> account_key{0x12, 0x22, 0x33, 0x44, 0x55, 0x66,
                                         0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                         0xCC, 0xDD, 0xEE, 0xFF};

  EXPECT_FALSE(AccountKeyFilter(kFilterBytes1, {salt}).IsAccountKeyInFilter(account_key));
  EXPECT_FALSE(AccountKeyFilter(kFilterBytes1And2, {salt}).IsAccountKeyInFilter(account_key));
}

TEST_F(AccountKeyFilterTest, WithBatteryData) {
  std::vector<uint8_t> salt_values = {salt};
  for (auto& byte : kBatteryData)
    salt_values.push_back(byte);

  EXPECT_TRUE(
      AccountKeyFilter(kFilterWithBattery, salt_values).IsAccountKeyInFilter(kAccountKey1));
}

TEST_F(AccountKeyFilterTest, TwoSaltBytes) {
  // Devices with battery data create account filters by concatenating data to
  // end of salt bytes
  std::vector<uint8_t> salt_values{};
  salt_values.insert(salt_values.end(), kSalt3.begin(), kSalt3.end());
  salt_values.insert(salt_values.end(), kBatteryData3.begin(),
                     kBatteryData3.end());

  EXPECT_TRUE(AccountKeyFilter(kFilter3, salt_values).IsAccountKeyInFilter(kAccountKey3));
}

// Regression test for b/243855406.
TEST_F(AccountKeyFilterTest, SassEnabledPeripheral) {
  // Account Key, Salt, and Battery Data values used are specific to an observed
  // SASS device failure explained at the variable declaration for
  // |kAccountKey4|, |kSalt4|, and |kBatteryData4|.
  std::vector<uint8_t> salt_values{};
  salt_values.insert(salt_values.end(), kSalt4.begin(), kSalt4.end());
  salt_values.insert(salt_values.end(), kBatteryData4.begin(),
                     kBatteryData4.end());
  EXPECT_TRUE(AccountKeyFilter(kFilter4, salt_values).IsAccountKeyInFilter(kAccountKey4));
}

}  // namespace quick_pair
}  // namespace ash
