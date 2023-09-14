// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREFS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREFS_H_

class PrefRegistrySimple;

namespace plus_addresses {
namespace prefs {
// An unsynced time pref keeping track of the last time the PlusAddress service
// periodically fetched all plus addresses from the remote server.
inline constexpr char kPlusAddressLastFetchedTime[] =
    "plus_address.last_fetched_time";
}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry);
}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREFS_H_
