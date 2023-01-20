// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/lacros_availability.h"

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"

namespace ash::standalone_browser {
namespace {

// The conversion map for LacrosAvailability policy data. The values must match
// the ones from LacrosAvailability.yaml.
constexpr auto kLacrosAvailabilityMap =
    base::MakeFixedFlatMap<base::StringPiece, LacrosAvailability>({
        {"user_choice", LacrosAvailability::kUserChoice},
        {"lacros_disallowed", LacrosAvailability::kLacrosDisallowed},
        {"side_by_side", LacrosAvailability::kSideBySide},
        {"lacros_primary", LacrosAvailability::kLacrosPrimary},
        {"lacros_only", LacrosAvailability::kLacrosOnly},
    });

}  // namespace

absl::optional<LacrosAvailability> ParseLacrosAvailability(
    base::StringPiece value) {
  auto* it = kLacrosAvailabilityMap.find(value);
  if (it != kLacrosAvailabilityMap.end()) {
    return it->second;
  }

  LOG(ERROR) << "Unknown LacrosAvailability policy value is passed: " << value;
  return absl::nullopt;
}

base::StringPiece GetLacrosAvailabilityPolicyName(LacrosAvailability value) {
  for (const auto& entry : kLacrosAvailabilityMap) {
    if (entry.second == value) {
      return entry.first;
    }
  }

  NOTREACHED();
  return base::StringPiece();
}

}  // namespace ash::standalone_browser
