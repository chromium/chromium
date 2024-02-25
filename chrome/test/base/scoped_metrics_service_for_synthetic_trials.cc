// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_metrics_service_for_synthetic_trials.h"

ScopedMetricsServiceForSyntheticTrials::ScopedMetricsServiceForSyntheticTrials(
    TestingBrowserProcess* testing_browser_process)
    : browser_process_(testing_browser_process) {
  CHECK(browser_process_);

  auto* local_state = browser_process_->local_state();
  CHECK(local_state)
      << "Error: local state prefs are required. In a unit test, this can be "
         "set up using base::test::ScopedFeatureList.";

  // The `SyntheticTrialsActiveGroupIdProvider` needs to be notified of
  // changes from the registry for them to be used through the variations API.
  synthetic_trial_registry_observation_.Observe(&synthetic_trial_registry_);

  metrics_service_client_.set_synthetic_trial_registry(
      &synthetic_trial_registry_);

  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      local_state, &enabled_state_provider_,
      /*backup_registry_key=*/std::wstring(),
      /*user_data_dir=*/base::FilePath());

  // Needs to be set up, will be updated at each synthetic trial change.
  variations::InitCrashKeys();

  // Required by `MetricsService` to record UserActions. We don't rely on
  // these here, since we never make it start recording metrics, but the task
  // runner is still required during the shutdown sequence.
  base::SetRecordActionTaskRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  metrics_service_ = std::make_unique<metrics::MetricsService>(
      metrics_state_manager_.get(), &metrics_service_client_, local_state);

  browser_process_->SetMetricsService(metrics_service_.get());
}

ScopedMetricsServiceForSyntheticTrials::
    ~ScopedMetricsServiceForSyntheticTrials() {
  // The scope is closing, undo the set up that was done in the constructor:
  // `MetricsService` and other necessary parts like crash keys.
  browser_process_->SetMetricsService(nullptr);
  variations::ClearCrashKeysInstanceForTesting();

  // Note: Clears all the synthetic trials, not juste the ones registered
  // during the lifetime of this object.
  variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
      ->ResetForTesting();
}
