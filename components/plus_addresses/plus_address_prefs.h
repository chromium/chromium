// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREFS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREFS_H_

#include <string_view>

class PrefRegistrySimple;

namespace plus_addresses::prefs {

// The version of the data format in which pre-allocated plus addresses are
// saved.
inline constexpr std::string_view kPreallocatedAddressesVersion =
    "plus_addresses.preallocation.version";

// The list of pre-allocated plus addresses.
inline constexpr std::string_view kPreallocatedAddresses =
    "plus_addresses.preallocation.addresses";

// The index of the next pre-allocated plus address to show.
inline constexpr std::string_view kPreallocatedAddressesNext =
    "plus_addresses.preallocation.next";

// The first time the user creates a plus address in Chrome.
inline constexpr std::string_view kFirstPlusAddressCreationTime =
    "plus_addresses.creation.first.time";

// The last time the user filled a plus address in Chrome.
inline constexpr std::string_view kLastPlusAddressFillingTime =
    "plus_addresses.last.filling.time";

// Registers the plus address profile prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace plus_addresses::prefs

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREFS_H_
