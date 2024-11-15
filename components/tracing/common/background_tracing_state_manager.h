// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_STATE_MANAGER_H_
#define COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_STATE_MANAGER_H_

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/tracing_export.h"

namespace tracing {

// Manages local state prefs for background tracing, and tracks state from
// previous background tracing session(s). All the calls are expected to run on
// UI thread.
class TRACING_EXPORT BackgroundTracingStateManager {
 public:
  static std::unique_ptr<BackgroundTracingStateManager> CreateInstance(
      PrefService* local_state);
  static BackgroundTracingStateManager& GetInstance();
  ~BackgroundTracingStateManager();

  // Saves user-controlled prefs related to tracing.
  // `enabled_scenario_hashes` is a list of hashes uniquely identifying scenario
  // configs.
  void UpdateEnabledScenarios(std::vector<std::string> enabled_scenario_hashes);
  void UpdatePrivacyFilter(bool enabled);

  const std::vector<std::string>& enabled_scenarios() const {
    return enabled_scenarios_;
  }
  bool privacy_filter_enabled() const { return privacy_filter_enabled_; }

  // Used in tests to reset the state since a singleton instance is never
  // destroyed.
  void ResetForTesting();

 private:
  explicit BackgroundTracingStateManager(PrefService* local_state);

  void Initialize();
  void SaveState();

  raw_ptr<PrefService> local_state_ = nullptr;
  std::vector<std::string> enabled_scenarios_;
  bool privacy_filter_enabled_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_STATE_MANAGER_H_
