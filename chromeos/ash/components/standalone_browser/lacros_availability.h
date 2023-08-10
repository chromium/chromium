// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

// When this feature is enabled, Lacros is allowed to roll out by policy to
// Googlers.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
BASE_DECLARE_FEATURE(kLacrosGooglePolicyRollout);

// Parses the string representation of LacrosAvailability policy value into
// the enum value. Returns nullopt on unknown value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
absl::optional<LacrosAvailability> ParseLacrosAvailability(
    base::StringPiece value);

// Returns the policy value name from the given value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
base::StringPiece GetLacrosAvailabilityPolicyName(LacrosAvailability value);

// Given a raw policy value, decides what LacrosAvailability value should be
// used as a result of policy application.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
LacrosAvailability DetermineLacrosAvailabilityFromPolicyValue(
    const user_manager::User* user,
    base::StringPiece policy_value);

// Returns true if the given user's profile is associated with a google internal
// account.
// TODO(andreaorru): conceptually, this is an internal utility function
// and should not be exported. Currently, `crosapi::browser_util` still
// depends on it. Remove once the IsLacrosEnabled* refactoring is complete.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
bool IsGoogleInternal(const user_manager::User* user);

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_
