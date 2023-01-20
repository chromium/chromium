// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::standalone_browser {

// Represents the policy indicating how to launch Lacros browser, named
// LacrosAvailability. The values shall be consistent with the controlling
// policy.
enum class LacrosAvailability {
  // Indicates that the user decides whether to enable Lacros (if allowed) and
  // make it the primary/only browser.
  kUserChoice = 0,
  // Indicates that Lacros is not allowed to be enabled.
  kLacrosDisallowed = 1,
  // Indicates that Lacros will be enabled (if allowed). Ash browser is the
  // primary browser.
  kSideBySide = 2,
  // Similar to kSideBySide but Lacros is the primary browser.
  kLacrosPrimary = 3,
  // Indicates that Lacros (if allowed) is the only available browser.
  kLacrosOnly = 4
};

// Parses the string representation of LacrosAvailability policy value into
// the enum value. Returns nullopt on unknown value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
absl::optional<LacrosAvailability> ParseLacrosAvailability(
    base::StringPiece value);

// Returns the policy value name from the given value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
base::StringPiece GetLacrosAvailabilityPolicyName(LacrosAvailability value);

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_LACROS_AVAILABILITY_H_
