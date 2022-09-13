// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_REPORTING_DEFAULT_STATE_H_
#define COMPONENTS_METRICS_METRICS_REPORTING_DEFAULT_STATE_H_

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// Metrics reporting default state. This relates to the state of the enable
// checkbox shown on first-run. This enum is used to store values in a pref, and
// shouldn't be renumbered.
enum EnableMetricsDefault {
  // We only record the value during first-run. The default of existing
  // installs is considered unknown.
  DEFAULT_UNKNOWN,
  // The first-run checkbox was unchecked by default.
  OPT_IN,
  // The first-run checkbox was checked by default.
  OPT_OUT,
};

// Register prefs relating to metrics reporting state. Currently only registers
// a pref for metrics reporting default opt-in state.
void RegisterMetricsReportingStatePrefs(PrefRegistrySimple* registry);

// Sets whether metrics reporting was opt-in or not. If it was opt-in, then the
// enable checkbox on first-run was default unchecked. If it was opt-out, then
// the checkbox was default checked. This should only be set once, and only
// during first-run.
void RecordMetricsReportingDefaultState(PrefService* local_state,
                                        EnableMetricsDefault default_state);

// Same as above, but does not verify the current state is UNKNOWN.
void ForceRecordMetricsReportingDefaultState(
    PrefService* local_state,
    EnableMetricsDefault default_state);

// Gets information about the default value for the enable metrics reporting
// checkbox shown during first-run.
EnableMetricsDefault GetMetricsReportingDefaultState(PrefService* local_state);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_REPORTING_DEFAULT_STATE_H_
