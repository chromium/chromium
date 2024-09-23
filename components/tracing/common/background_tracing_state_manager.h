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

// Do not remove or change the order of enum fields since it is stored in
// preferences.
enum class BackgroundTracingState : int {
  // Default state when tracing is not started in previous session, or when
  // state is not found or invalid.
  NOT_ACTIVATED = 0,
  STARTED = 1,
  RAN_30_SECONDS = 2,
  FINALIZATION_STARTED = 3,
  LAST = FINALIZATION_STARTED,
};

// Manages local state prefs for background tracing, and tracks state from
// previous background tracing session(s). All the calls are expected to run on
// UI thread.
class TRACING_EXPORT BackgroundTracingStateManager {
 public:
  static std::unique_ptr<BackgroundTracingStateManager> CreateInstance(
      PrefService* local_state);
  static BackgroundTracingStateManager& GetInstance();
  ~BackgroundTracingStateManager();

  // True if last session potentially crashed and it is unsafe to turn on
  // background tracing in current session.
  bool DidLastSessionEndUnexpectedly() const;

  // The embedder should call this method every time background tracing starts
  // so that the state in prefs is updated. Posts a timer task to the current
  // sequence to update the state once more to denote no crashes after a
  // reasonable time (see DidLastSessionEndUnexpectedly()).
  void OnTracingStarted();
  void OnTracingStopped();

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

  // Updates the current tracing state and saves it to prefs.
  void SetState(BackgroundTracingState new_state);

  BackgroundTracingState state_ = BackgroundTracingState::NOT_ACTIVATED;

  raw_ptr<PrefService> local_state_ = nullptr;
  std::vector<std::string> enabled_scenarios_;
  bool privacy_filter_enabled_ = true;

  BackgroundTracingState last_session_end_state_ =
      BackgroundTracingState::NOT_ACTIVATED;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_STATE_MANAGER_H_
