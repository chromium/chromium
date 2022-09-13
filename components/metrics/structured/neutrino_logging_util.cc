// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/neutrino_logging_util.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/structured/structured_events.h"

namespace metrics {
namespace structured {

void NeutrinoDevicesLogWithLocalState(PrefService* local_state,
                                      NeutrinoDevicesLocation location) {
  auto code_point = events::v2::neutrino_devices::CodePoint();
  if (local_state &&
      local_state->HasPrefPath(metrics::prefs::kMetricsClientID)) {
    const std::string client_id =
        local_state->GetString(metrics::prefs::kMetricsClientID);
    if (!client_id.empty())
      code_point.SetClientId(client_id);
  }
  code_point.SetLocation(static_cast<int64_t>(location)).Record();
}

void NeutrinoDevicesLogEnrollmentWithLocalState(
    PrefService* local_state,
    bool is_managed,
    NeutrinoDevicesLocation location) {
  auto enrollment = events::v2::neutrino_devices::Enrollment();
  if (local_state &&
      local_state->HasPrefPath(metrics::prefs::kMetricsClientID)) {
    const std::string client_id =
        local_state->GetString(metrics::prefs::kMetricsClientID);
    if (!client_id.empty())
      enrollment.SetClientId(client_id);
  }
  enrollment.SetIsManagedPolicy(is_managed)
      .SetLocation(static_cast<int64_t>(location))
      .Record();
}

}  // namespace structured
}  // namespace metrics
