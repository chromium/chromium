// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_QUICK_START_DECODER_QUICK_START_CONVERSIONS_H_
#define CHROME_SERVICES_SHARING_NEARBY_QUICK_START_DECODER_QUICK_START_CONVERSIONS_H_

#include <string_view>

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"

namespace ash::quick_start {

// Converts a string into a WifiSecurityType enum, or returns nullopt if
// it's not a valid security type.
std::optional<mojom::WifiSecurityType> WifiSecurityTypeFromString(
    std::string_view security_type_string);

}  // namespace ash::quick_start

#endif  // CHROME_SERVICES_SHARING_NEARBY_QUICK_START_DECODER_QUICK_START_CONVERSIONS_H_
