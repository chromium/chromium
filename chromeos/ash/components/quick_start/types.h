// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "base/types/strong_alias.h"

// This file contains user defined type aliases that are used throughout Quick
// Start to ensure strong typing of encoded data. Using types like `std::string`
// doesn't tell us if we dealing with raw bytes or Base64 encoded data or some
// other encoding.

namespace ash::quick_start {

// A Base64 encoded string.
using Base64String = base::StrongAlias<class Base64StringTag, std::string>;

// A Base64Url encoded string.
using Base64UrlString =
    base::StrongAlias<class Base64UrlStringTag, std::string>;

// A PEM encoded certificate chain.
using PEMCertChain = base::StrongAlias<class PEMCertChainTag, std::string>;

// A string containing the PIN to be shown on the QuickStart UI.
using PinString = base::StrongAlias<class PinStringTag, std::string>;

// A string containing the user's email being transferred from the phone.
using EmailString = base::StrongAlias<class EmailStringTag, std::string>;

// Encodes a byte array to a Base64Url encoded string. Omits padding characters
// in the output.
Base64UrlString Base64UrlEncode(const std::vector<uint8_t>& data);

// Same as above - except that it accepts `data` as `std::string`.
Base64UrlString Base64UrlEncode(const std::string& data);

// Transcodes a Base64 encoded string to Base64Url. Returns an empty optional if
// the input string is incorrectly encoded. Omits padding characters in the
// output.
std::optional<Base64UrlString> Base64UrlTranscode(const Base64String& data);

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_TYPES_H_
