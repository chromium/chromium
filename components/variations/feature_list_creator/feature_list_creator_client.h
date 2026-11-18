// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_FEATURE_LIST_CREATOR_FEATURE_LIST_CREATOR_CLIENT_H_
#define COMPONENTS_VARIATIONS_FEATURE_LIST_CREATOR_FEATURE_LIST_CREATOR_CLIENT_H_

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "components/crash/core/common/crash_keys.h"

namespace base {
class CommandLine;
}  // namespace base

namespace flags_ui {
class FlagsState;
class FlagsStorage;
}  // namespace flags_ui

namespace metrics_services_manager {
class MetricsServicesManager;
}  // namespace metrics_services_manager

namespace variations {

class PlatformFieldTrials;

// Supplies embedder-specific objects and hooks to `CreateFeatureList()`.
// Chrome and iOS implement `FeatureListCreatorClient`.
class FeatureListCreatorClient {
 public:
  virtual ~FeatureListCreatorClient();

  // Returns the `flags_ui::FlagsState` for `about:flags` entries.
  virtual flags_ui::FlagsState& GetFlagsState() = 0;

  // Creates the `flags_ui::FlagsStorage` that reads active `about:flags`
  // entries. Local State exists when `CreateFeatureList()` calls
  // `CreateFlagsStorage()`. Implementations must return a non-null
  // `FlagsStorage` instance.
  virtual std::unique_ptr<flags_ui::FlagsStorage> CreateFlagsStorage() = 0;

  // Returns the switch filter for `crash_keys::SetSwitchesFromCommandLine()`.
  // Returns `&crash_keys::IsDefaultBoringSwitch` by default.
  virtual crash_keys::SwitchFilterFunction GetSwitchFilter();

  // Returns the `MetricsServicesManager`, creating it if needed. The embedder
  // retains ownership, and the `MetricsServicesManager` must outlive
  // `CreateFeatureList()`.
  virtual metrics_services_manager::MetricsServicesManager&
  GetOrCreateMetricsServicesManager() = 0;

  // Runs after `about:flags` parameter registration and before field trial
  // setup. Embedders can modify `feature_list` in `WillSetUpFieldTrials()`.
  virtual void WillSetUpFieldTrials(base::FeatureList& feature_list) {}

  // Returns the `PlatformFieldTrials` instance for field trial setup.
  virtual PlatformFieldTrials& GetPlatformFieldTrials() = 0;

  // Computes extra feature overrides from `command_line` after `about:flags`
  // switch conversion. `VariationsService` applies the extra overrides after
  // `--enable-features` and `--disable-features`, and before field trials.
  virtual std::vector<base::FeatureList::FeatureOverrideInfo>
  GetExtraFeatureOverrides(const base::CommandLine& command_line);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_FEATURE_LIST_CREATOR_FEATURE_LIST_CREATOR_CLIENT_H_
