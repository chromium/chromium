// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_HATS_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_HATS_UTILS_H_

namespace plus_addresses::hats {

// Hats Bits data fields:
inline constexpr char kPlusAddressesCount[] =
    "The number of the plus addresses the user has";
inline constexpr char kFirstPlusAddressCreationTime[] =
    "Time passed since the user has created the first plus address, in seconds";
inline constexpr char kLastPlusAddressFillingTime[] =
    "Time passed since the user has filled a plus address the last time, in "
    "seconds";

// Plus address survey parameters:
//
// The custom survey cooldown override for plus addresses HaTS surveys. The
// survey can be triggered after this cooldown period instead of the default
// 180 days delay.
inline constexpr char kCooldownOverrideDays[] = "cooldown-override-days";
// The lower bound on the plus address survey delay.
inline constexpr char kMinDelayMs[] = "min-delay-ms";
// The upper bound on the plus address survey delay.
inline constexpr char kMaxDelayMs[] = "max-delay-ms";

}  // namespace plus_addresses::hats

#endif  // COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_HATS_UTILS_H_
