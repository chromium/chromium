// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HATS_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HATS_UTILS_H_

namespace plus_addresses::hats {

// Hats Bits data fields:
inline constexpr char kFirstPlusAddressCreationTime[] =
    "Time passed since the user has created the first plus address, in seconds";
inline constexpr char kLastPlusAddressFillingTime[] =
    "Time passed since the user has filled a plus address the last time, in "
    "seconds";

}  // namespace plus_addresses::hats

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HATS_UTILS_H_
