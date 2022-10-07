// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/neutrino_logging.h"

#include "base/time/time.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"

namespace {

// Return the integer floor of the log base 2 of the time since |timestamp|,
// measured in days.
int Log2TimeSince(int64_t timestamp) {
  return floor(
      log2((base::Time::Now() - base::Time::FromTimeT(timestamp)).InSecondsF() /
           base::Days(1).InSecondsF()));
}

}  // namespace

namespace metrics {
namespace structured {

void NeutrinoDevicesLog(NeutrinoDevicesLocation location) {
  NeutrinoDevicesLogWithClientId("", location);
}

void NeutrinoDevicesLogWithClientId(const std::string& client_id,
                                    NeutrinoDevicesLocation location) {
  events::v2::neutrino_devices::CodePoint code_point;
  if (!client_id.empty())
    code_point.SetClientId(client_id);
  code_point.SetLocation(static_cast<int64_t>(location)).Record();
}

void NeutrinoDevicesLogPolicy(const std::string& client_id,
                              bool is_managed,
                              NeutrinoDevicesLocation location) {
  events::v2::neutrino_devices::Enrollment enrollment;
  if (!client_id.empty())
    enrollment.SetClientId(client_id);
  enrollment.SetIsManagedPolicy(is_managed)
      .SetLocation(static_cast<int64_t>(location))
      .Record();
}

void NeutrinoDevicesLogClientIdCleared(
    const std::string& client_id,
    int64_t install_date_timestamp,
    int64_t metrics_reporting_enabled_timestamp) {
  events::v2::neutrino_devices::ClientIdCleared client_id_cleared;
  if (!client_id.empty())
    client_id_cleared.SetInitialClientId(client_id);
  client_id_cleared
      .SetLog2TimeSinceInstallation(Log2TimeSince(install_date_timestamp))
      .SetLog2TimeSinceMetricsEnabled(
          Log2TimeSince(metrics_reporting_enabled_timestamp))
      .Record();
}

void NeutrinoDevicesLogClientIdChanged(
    const std::string& client_id,
    const std::string& initial_client_id,
    int64_t install_date_timestamp,
    int64_t metrics_reporting_enabled_timestamp,
    NeutrinoDevicesLocation location) {
  events::v2::neutrino_devices::ClientIdChanged client_id_changed;

  if (!initial_client_id.empty())
    client_id_changed.SetInitialClientId(initial_client_id);
  if (!client_id.empty())
    client_id_changed.SetFinalClientId(client_id);

  const absl::optional<int> last_key_rotation =
      Recorder::GetInstance()->LastKeyRotation(client_id_changed);

  if (last_key_rotation.has_value()) {
    int days_since_rotation =
        (base::Time::Now() - base::Time::UnixEpoch()).InDays() -
        last_key_rotation.value();
    client_id_changed.SetDaysSinceKeyRotation(days_since_rotation);
  }

  client_id_changed
      .SetLog2TimeSinceInstallation(Log2TimeSince(install_date_timestamp))
      .SetLog2TimeSinceMetricsEnabled(
          Log2TimeSince(metrics_reporting_enabled_timestamp))
      .SetLocation(static_cast<int64_t>(location))
      .Record();
}

}  // namespace structured
}  // namespace metrics
