// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_SELECTION_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_SELECTION_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"

namespace policy {
class PolicyMap;
}  // namespace policy

namespace ash::standalone_browser {

// Represents the policy indicating which Lacros browser to launch, named
// LacrosSelection. The values shall be consistent with the controlling
// policy. Unlike `LacrosSelection` representing which lacros to select,
// `LacrosSelectionPolicy` represents how to decide which lacros to select.
// Stateful option from `LacrosSelection` is omitted due to a breakage risks in
// case of version skew (e.g. when the latest stateful Lacros available in omaha
// is older than the rootfs Lacros on the device).
enum class LacrosSelectionPolicy {
  // Indicates that the user decides which Lacros browser to launch: rootfs or
  // stateful.
  kUserChoice = 0,
  // Indicates that rootfs Lacros will always be launched.
  kRootfs = 1,
};

// Represents the different options available for lacros selection.
enum class LacrosSelection {
  kRootfs = 0,
  kStateful = 1,
  kDeployedLocally = 2,
  kMaxValue = kDeployedLocally,
};

// A command-line switch that can also be set from chrome://flags that chooses
// which selection of Lacros to use.
inline constexpr char kLacrosSelectionSwitch[] = "lacros-selection";
inline constexpr char kLacrosSelectionRootfs[] = "rootfs";
inline constexpr char kLacrosSelectionStateful[] = "stateful";

// To be called at primary user login, to cache the policy value for
// LacrosSelection policy. The effective value of the policy does not
// change for the duration of the user session, so cached value shall be
// checked.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void CacheLacrosSelection(const policy::PolicyMap& map);

// Returns cached value of LacrosSelection policy. See `CacheLacrosSelection`
// for details.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
LacrosSelectionPolicy GetCachedLacrosSelectionPolicy();

// Returns lacros selection option according to LarcrosSelectionPolicy and
// lacros-selection flag. Returns nullopt if there is no preference.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
std::optional<LacrosSelection> DetermineLacrosSelection();

// Parses the string representation of LacrosSelection policy value into the
// enum value. Returns nullopt on unknown value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
std::optional<LacrosSelectionPolicy> ParseLacrosSelectionPolicy(
    std::string_view value);

// Returns the LacrosSelection policy value name from the given value. Returned
// std::string_view is guaranteed to never be invalidated.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
std::string_view GetLacrosSelectionPolicyName(LacrosSelectionPolicy value);

// Clears the cached value for LacrosSelection policy.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void ClearLacrosSelectionCacheForTest();

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_SELECTION_H_
