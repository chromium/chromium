// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/public/cpp/account_key_filter.h"

#include <math.h>

#include <cstddef>
#include <iterator>

#include "base/ranges/algorithm.h"
#include "chromeos/ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "crypto/sha2.h"

namespace ash {
namespace quick_pair {

namespace {

constexpr int kBitsInByte = 8;

// SASS enabled peripherals create their Bloom filters by changing the first
// byte of either the account key in use or the most recently used account key
// according to this spec:
// https://developers.google.com/nearby/fast-pair/early-access/specifications/extensions/sass#SassInUseAccountKey
constexpr uint8_t kRecentlyUsedByte = 0x05;
constexpr uint8_t kInUseByte = 0x06;

// Helper to AccountKeyFilter::IsAccountKeyInFilter().
// Performs the test to see if |data| is in |bit_sets|, a Bloom filter.
bool AccountKeyFilterTest(const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& bit_sets) {
  std::array<uint8_t, 32> hashed = crypto::SHA256Hash(data);

  // Iterate over the hashed input in 4 byte increments, combine those 4
  // bytes into an unsigned int and use it as the index into our
  // |bit_sets|.
  for (size_t i = 0; i < hashed.size(); i += 4) {
    uint32_t hash = uint32_t{hashed[i]} << 24 | uint32_t{hashed[i + 1]} << 16 |
                    uint32_t{hashed[i + 2]} << 8 | hashed[i + 3];

    size_t num_bits = bit_sets.size() * kBitsInByte;
    size_t n = hash % num_bits;
    size_t byte_index = floor(n / kBitsInByte);
    size_t bit_index = n % kBitsInByte;
    bool is_set = (bit_sets[byte_index] >> bit_index) & 0x01;

    if (!is_set)
      return false;
  }

  return true;
}

}  // namespace

AccountKeyFilter::AccountKeyFilter(
    const NotDiscoverableAdvertisement& advertisement)
    : bit_sets_(advertisement.account_key_filter) {
  salt_values_.resize(advertisement.salt.size());
  base::ranges::copy(advertisement.salt, salt_values_.begin());

  // If the advertisement contains battery information, then that information
  // was also appended to the account keys to generate the filter. We need to
  // do the same when checking for matches, so save the values in salt_values_
  // for that purpose later.
  if (advertisement.battery_notification) {
    salt_values_.push_back(
        advertisement.battery_notification->show_ui ? 0b00110011 : 0b00110100);

    salt_values_.push_back(
        advertisement.battery_notification->left_bud_info.ToByte());

    salt_values_.push_back(
        advertisement.battery_notification->right_bud_info.ToByte());

    salt_values_.push_back(
        advertisement.battery_notification->case_info.ToByte());
  }
}

AccountKeyFilter::AccountKeyFilter(
    const std::vector<uint8_t>& account_key_filter_bytes,
    const std::vector<uint8_t>& salt_values)
    : bit_sets_(account_key_filter_bytes), salt_values_(salt_values) {}

AccountKeyFilter::AccountKeyFilter(const AccountKeyFilter&) = default;
AccountKeyFilter& AccountKeyFilter::operator=(AccountKeyFilter&&) = default;
AccountKeyFilter::~AccountKeyFilter() = default;

bool AccountKeyFilter::IsAccountKeyInFilter(
    const std::vector<uint8_t>& account_key_bytes) const {
  if (bit_sets_.empty())
    return false;

  // We first need to append the salt value to the input (see
  // https://developers.google.com/nearby/fast-pair/spec#AccountKeyFilter).
  std::vector<uint8_t> default_account_key(account_key_bytes);
  for (auto& byte : salt_values_)
    default_account_key.push_back(byte);

  // We need to try account keys with different first bytes in case
  // the peripheral is SASS per
  // https://developers.google.com/nearby/fast-pair/early-access/specifications/extensions/sass#SassAdvertisingPayload
  std::vector<uint8_t> recently_used_account_key(default_account_key);
  recently_used_account_key[0] = kRecentlyUsedByte;

  std::vector<uint8_t> in_use_account_key(default_account_key);
  in_use_account_key[0] = kInUseByte;

  return AccountKeyFilterTest(default_account_key, bit_sets_) ||
         AccountKeyFilterTest(in_use_account_key, bit_sets_) ||
         AccountKeyFilterTest(recently_used_account_key, bit_sets_);
}

}  // namespace quick_pair
}  // namespace ash
