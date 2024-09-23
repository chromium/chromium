// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_conversions.h"

#include <string_view>

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"

namespace ash::quick_start {

namespace {
// The following are string representations of expected Wifi security types
constexpr char kPSK[] = "PSK";
constexpr char kWEP[] = "WEP";
constexpr char kEAP[] = "EAP";
constexpr char kOpen[] = "Open";
constexpr char kOWE[] = "OWE";
constexpr char kSAE[] = "SAE";
constexpr char kUnsupported[] = "Unsupported";
}  // namespace

std::optional<mojom::WifiSecurityType> WifiSecurityTypeFromString(
    std::string_view security_type_string) {
  if (security_type_string == kPSK) {
    return mojom::WifiSecurityType::kPSK;
  }

  if (security_type_string == kWEP) {
    return mojom::WifiSecurityType::kWEP;
  }

  if (security_type_string == kEAP) {
    return mojom::WifiSecurityType::kEAP;
  }

  if (security_type_string == kOpen) {
    return mojom::WifiSecurityType::kOpen;
  }

  if (security_type_string == kOWE) {
    return mojom::WifiSecurityType::kOWE;
  }

  if (security_type_string == kSAE) {
    return mojom::WifiSecurityType::kSAE;
  }

  if (security_type_string == kUnsupported) {
    LOG(ERROR) << "Unsupported security type!";
  }

  return std::nullopt;
}

}  // namespace ash::quick_start
