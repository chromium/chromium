// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_TYPES_H_

#include <string>
#include <vector>

#include "base/types/strong_alias.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

// Encodes a byte array to a Base64Url encoded string. Omits padding characters
// in the output.
Base64UrlString Base64UrlEncode(const std::vector<uint8_t>& data);

// Transcodes a Base64 encoded string to Base64Url. Returns an empty optional if
// the input string is incorrectly encoded. Omits padding characters in the
// output.
absl::optional<Base64UrlString> Base64UrlTranscode(const Base64String& data);

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_TYPES_H_
