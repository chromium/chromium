// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/feature_list.h"

namespace policy {
class PolicyMap;
}

namespace user_manager {
class User;
}

namespace ash::standalone_browser {
// Represents the policy indicating how to launch Lacros browser, named
// LacrosAvailability. The values shall be consistent with the controlling
// policy.
// Values 2 and 3 were removed and should not be reused.
enum class LacrosAvailability {
  // Indicates that the user decides whether to enable Lacros (if allowed) and
  // make it the primary/only browser.
  kUserChoice = 0,
  // Indicates that Lacros is not allowed to be enabled.
  kLacrosDisallowed = 1,
  // Indicates that Lacros (if allowed) is the only available browser.
  kLacrosOnly = 4
};

// The internal name in about_flags.cc for the lacros-availablility-policy
// config.
inline constexpr char kLacrosAvailabilityPolicyInternalName[] =
    "lacros-availability-policy";

// The commandline flag name of lacros-availability-policy.
// The value should be the policy value as defined just below.
// The values need to be consistent with kLacrosAvailabilityMap above.
inline constexpr char kLacrosAvailabilityPolicySwitch[] =
    "lacros-availability-policy";
inline constexpr char kLacrosAvailabilityPolicyUserChoice[] = "user_choice";
inline constexpr char kLacrosAvailabilityPolicyLacrosDisabled[] =
    "lacros_disabled";
inline constexpr char kLacrosAvailabilityPolicyLacrosOnly[] = "lacros_only";

// When this feature is enabled, Lacros is allowed to roll out by policy to
// Googlers.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
BASE_DECLARE_FEATURE(kLacrosGooglePolicyRollout);

// Parses the string representation of LacrosAvailability policy value into
// the enum value. Returns nullopt on unknown value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
std::optional<LacrosAvailability> ParseLacrosAvailability(
    std::string_view value);

// Returns the policy value name from the given value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
std::string_view GetLacrosAvailabilityPolicyName(LacrosAvailability value);

// Given a raw policy value, decides what LacrosAvailability value should be
// used as a result of policy application.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
LacrosAvailability DetermineLacrosAvailabilityFromPolicyValue(
    const user_manager::User* user,
    std::string_view policy_value);

// Returns LacrosAvailability policy for the given `user` and its `policy_map`.
// This function may take a look at more surrounding context.
LacrosAvailability GetLacrosAvailability(const user_manager::User* user,
                                         const policy::PolicyMap& policy_map);

// Returns true if the given user's profile is associated with a google internal
// account. This includes @managedchrome.com accounts.
// TODO(andreaorru): conceptually, this is an internal utility function
// and should not be exported. Currently, `crosapi::browser_util` still
// depends on it. Remove once the IsLacrosEnabled* refactoring is complete.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
bool IsGoogleInternal(const user_manager::User* user);

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_
