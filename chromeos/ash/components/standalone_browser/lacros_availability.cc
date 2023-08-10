// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/lacros_availability.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash::standalone_browser {
namespace {

// The conversion map for LacrosAvailability policy data. The values must match
// the ones from LacrosAvailability.yaml.
// TODO(crbug.com/1448575): Remove the side_by_side and lacros_primary values
// from the policy.
constexpr auto kLacrosAvailabilityMap =
    base::MakeFixedFlatMap<base::StringPiece, LacrosAvailability>({
        {"user_choice", LacrosAvailability::kUserChoice},
        {"lacros_disallowed", LacrosAvailability::kLacrosDisallowed},
        {"side_by_side", LacrosAvailability::kLacrosDisallowed},
        {"lacros_primary", LacrosAvailability::kLacrosDisallowed},
        {"lacros_only", LacrosAvailability::kLacrosOnly},
    });

}  // namespace

BASE_FEATURE(kLacrosGooglePolicyRollout,
             "LacrosGooglePolicyRollout",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

bool IsGoogleInternal(const user_manager::User* user) {
  if (!user) {
    return false;
  }

  return gaia::IsGoogleInternalAccountEmail(
      user->GetAccountId().GetUserEmail());
}

LacrosAvailability DetermineLacrosAvailabilityFromPolicyValue(
    const user_manager::User* user,
    base::StringPiece policy_value) {
  // Users can set this switch in chrome://flags to disable the effect of the
  // lacros-availability policy. This should only be allowed for Googlers.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kLacrosAvailabilityIgnore) &&
      IsGoogleInternal(user)) {
    return LacrosAvailability::kUserChoice;
  }

  if (policy_value.empty()) {
    // Some tests call IsLacrosAllowedToBeEnabled but don't have the value set.
    return LacrosAvailability::kUserChoice;
  }

  auto result = ParseLacrosAvailability(policy_value);
  if (!result.has_value()) {
    return LacrosAvailability::kUserChoice;
  }

  if (IsGoogleInternal(user) &&
      !base::FeatureList::IsEnabled(kLacrosGooglePolicyRollout) &&
      result != LacrosAvailability::kLacrosDisallowed) {
    return LacrosAvailability::kUserChoice;
  }

  return result.value();
}

}  // namespace ash::standalone_browser
