// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HATS_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HATS_UTILS_H_

#include <map>
#include <string>

class PrefService;

namespace plus_addresses::hats {

// Hats Bits data fields:
inline constexpr char kFirstPlusAddressCreationTime[] =
    "Time passed since the user has created the first plus address, in seconds";
inline constexpr char kLastPlusAddressFillingTime[] =
    "Time passed since the user has filled a plus address the last time, in "
    "seconds";

// Specifies the type of feature perception flow to launch for the user.
enum class SurveyType {
  // Triggered after the user has created their first plus address.
  kAcceptedFirstTimeCreate = 1,
  // The user has declined the first plus address creation flow.
  kDeclinedFirstTimeCreate = 2,
  // The user has created their 3rd, 4th, ... plus address.
  kCreatedMultiplePlusAddresses = 3,
};

std::map<std::string, std::string> GetPlusAddressHatsData(
    PrefService* pref_service);

}  // namespace plus_addresses::hats

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HATS_UTILS_H_
