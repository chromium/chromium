// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/lacros_selection.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user_manager.h"

namespace ash::standalone_browser {

namespace {

// At session start the value for LacrosSelection logic is applied and the
// result is stored in this variable which is used after that as a cache.
// TODO(crbug.com/336839132): Move cache related methods to a wrapper class
// instead of global functions + global variables.
std::optional<LacrosSelectionPolicy> g_lacros_selection_cache;

// The conversion map for LacrosSelection policy data. The values must match
// the ones from LacrosSelection.yaml.
constexpr auto kLacrosSelectionPolicyMap =
    base::MakeFixedFlatMap<std::string_view, LacrosSelectionPolicy>({
        {"user_choice", LacrosSelectionPolicy::kUserChoice},
        {"rootfs", LacrosSelectionPolicy::kRootfs},
    });

}  // namespace

void CacheLacrosSelection(const policy::PolicyMap& map) {
  if (g_lacros_selection_cache.has_value()) {
    // Some browser tests might call this multiple times.
    LOG(ERROR) << "Trying to cache LacrosSelection and the value was set";
    return;
  }

  // Users can set this switch in chrome://flags to disable the effect of the
  // lacros-selection policy. This should only be allows for googlers.
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(ash::switches::kLacrosSelectionPolicyIgnore) &&
      IsGoogleInternal(user_manager::UserManager::Get()->GetPrimaryUser())) {
    LOG(WARNING) << "LacrosSelection policy is ignored due to the ignore flag";
    return;
  }

  const base::Value* value =
      map.GetValue(policy::key::kLacrosSelection, base::Value::Type::STRING);
  g_lacros_selection_cache = ParseLacrosSelectionPolicy(
      value ? value->GetString() : std::string_view());
}

LacrosSelectionPolicy GetCachedLacrosSelectionPolicy() {
  return g_lacros_selection_cache.value_or(LacrosSelectionPolicy::kUserChoice);
}

std::optional<LacrosSelection> DetermineLacrosSelection() {
  switch (GetCachedLacrosSelectionPolicy()) {
    case LacrosSelectionPolicy::kRootfs:
      return LacrosSelection::kRootfs;
    case LacrosSelectionPolicy::kUserChoice:
      break;
  }

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  if (!cmdline->HasSwitch(kLacrosSelectionSwitch)) {
    return std::nullopt;
  }

  auto value = cmdline->GetSwitchValueASCII(kLacrosSelectionSwitch);
  if (value == kLacrosSelectionRootfs) {
    return LacrosSelection::kRootfs;
  }
  if (value == kLacrosSelectionStateful) {
    return LacrosSelection::kStateful;
  }

  return std::nullopt;
}

std::optional<LacrosSelectionPolicy> ParseLacrosSelectionPolicy(
    std::string_view value) {
  auto it = kLacrosSelectionPolicyMap.find(value);
  if (it != kLacrosSelectionPolicyMap.end()) {
    return it->second;
  }

  LOG(ERROR) << "Unknown LacrosSelection policy value is passed: " << value;
  return std::nullopt;
}

std::string_view GetLacrosSelectionPolicyName(LacrosSelectionPolicy value) {
  for (const auto& entry : kLacrosSelectionPolicyMap) {
    if (entry.second == value) {
      return entry.first;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return std::string_view();
}

void ClearLacrosSelectionCacheForTest() {
  g_lacros_selection_cache.reset();
}

}  // namespace ash::standalone_browser
