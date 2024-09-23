// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/install_date_provider.h"

#include "base/logging.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

namespace metrics {

namespace {

// The following two methods are copied from metrics_state_manager.cc, but are
// simple enough it's not really needed to reuse.
int64_t ReadInstallDate(PrefService* local_state) {
  return local_state->GetInt64(prefs::kInstallDate);
}

// Round a timestamp measured in seconds since epoch to one with a granularity
// of an hour. This can be used before uploaded potentially sensitive
// timestamps.
int64_t RoundSecondsToHour(int64_t time_in_seconds) {
  return 3600 * (time_in_seconds / 3600);
}

}  // namespace

void InstallDateProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
  system_profile_proto->set_install_date(
      RoundSecondsToHour(ReadInstallDate(local_state_)));
}

}  // namespace metrics
