// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_default_state.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

void RegisterMetricsReportingStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kMetricsDefaultOptIn,
                                EnableMetricsDefault::DEFAULT_UNKNOWN);
}

void RecordMetricsReportingDefaultState(PrefService* local_state,
                                        EnableMetricsDefault default_state) {
  DCHECK_EQ(GetMetricsReportingDefaultState(local_state),
            EnableMetricsDefault::DEFAULT_UNKNOWN);
  local_state->SetInteger(prefs::kMetricsDefaultOptIn, default_state);
}

void ForceRecordMetricsReportingDefaultState(
    PrefService* local_state,
    EnableMetricsDefault default_state) {
  local_state->SetInteger(prefs::kMetricsDefaultOptIn, default_state);
}

EnableMetricsDefault GetMetricsReportingDefaultState(PrefService* local_state) {
  return static_cast<EnableMetricsDefault>(
      local_state->GetInteger(prefs::kMetricsDefaultOptIn));
}

}  // namespace metrics
