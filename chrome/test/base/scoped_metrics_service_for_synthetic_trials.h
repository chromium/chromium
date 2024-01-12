// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_SCOPED_METRICS_SERVICE_FOR_SYNTHETIC_TRIALS_H_
#define CHROME_TEST_BASE_SCOPED_METRICS_SERVICE_FOR_SYNTHETIC_TRIALS_H_

#include <memory>
#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_task_environment.h"

class ScopedMetricsServiceForSyntheticTrials {
 public:
  // Sets up a `metrics::MetricsService` instance and makes it available in its
  // scope via `testing_browser_process->metrics_service()`.
  //
  // This service only supports feature related to the usage of synthetic field
  // trials.
  //
  // Requires:
  // - the local state prefs to be usable from `testing_browser_process`
  // - a task runner to be available (see //docs/threading_and_tasks_testing.md)
  explicit ScopedMetricsServiceForSyntheticTrials(
      TestingBrowserProcess* testing_browser_process);

  ~ScopedMetricsServiceForSyntheticTrials();

  metrics::MetricsService* Get() { return metrics_service_.get(); }

 private:
  raw_ptr<TestingBrowserProcess> browser_process_ = nullptr;

  metrics::TestEnabledStateProvider enabled_state_provider_{/*consent=*/true,
                                                            /*enabled=*/true};

  variations::SyntheticTrialRegistry synthetic_trial_registry_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_registry_observation_{
          variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()};

  metrics::TestMetricsServiceClient metrics_service_client_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  std::unique_ptr<metrics::MetricsService> metrics_service_;
};

#endif  // CHROME_TEST_BASE_SCOPED_METRICS_SERVICE_FOR_SYNTHETIC_TRIALS_H_
