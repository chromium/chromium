// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/lacros_availability.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash::standalone_browser {
namespace {

// The conversion map for LacrosAvailability policy data. The values must match
// the ones from LacrosAvailability.yaml.
// TODO(crbug.com/40269372): Remove the side_by_side and lacros_primary values
// from the policy.
constexpr auto kLacrosAvailabilityMap =
    base::MakeFixedFlatMap<std::string_view, LacrosAvailability>({
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

std::optional<LacrosAvailability> ParseLacrosAvailability(
    std::string_view value) {
  auto it = kLacrosAvailabilityMap.find(value);
  if (it != kLacrosAvailabilityMap.end()) {
    return it->second;
  }

  LOG(ERROR) << "Unknown LacrosAvailability policy value is passed: " << value;
  return std::nullopt;
}

std::string_view GetLacrosAvailabilityPolicyName(LacrosAvailability value) {
  for (const auto& entry : kLacrosAvailabilityMap) {
    if (entry.second == value) {
      return entry.first;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return std::string_view();
}

bool IsGoogleInternal(const user_manager::User* user) {
  if (!user) {
    return false;
  }

  const std::string_view email = user->GetAccountId().GetUserEmail();
  return gaia::IsGoogleInternalAccountEmail(email) ||
         gaia::IsGoogleRobotAccountEmail(email) ||
         gaia::ExtractDomainName(gaia::SanitizeEmail(email)) ==
             "managedchrome.com";
}

LacrosAvailability DetermineLacrosAvailabilityFromPolicyValue(
    const user_manager::User* user,
    std::string_view policy_value) {
  // Users can set this switch in chrome://flags to disable the effect of the
  // lacros-availability policy. This should only be allowed for Googlers.
  // Note: Flag actively used by CfM to bypass LaCrOS Availability Policy.
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

LacrosAvailability GetLacrosAvailability(const user_manager::User* user,
                                         const policy::PolicyMap& policy_map) {
  const base::Value* value = policy_map.GetValue(
      policy::key::kLacrosAvailability, base::Value::Type::STRING);
  return DetermineLacrosAvailabilityFromPolicyValue(
      user, value ? value->GetString() : std::string_view());
}

}  // namespace ash::standalone_browser
