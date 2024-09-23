// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/nearby_periodic_scheduler.h"

#include <algorithm>
#include <utility>

namespace ash::nearby {

NearbyPeriodicScheduler::NearbyPeriodicScheduler(base::TimeDelta request_period,
                                                 bool retry_failures,
                                                 bool require_connectivity,
                                                 const std::string& pref_name,
                                                 PrefService* pref_service,
                                                 OnRequestCallback callback,
                                                 Feature logging_feature,
                                                 const base::Clock* clock)
    : NearbySchedulerBase(retry_failures,
                          require_connectivity,
                          pref_name,
                          pref_service,
                          std::move(callback),
                          logging_feature,
                          clock),
      request_period_(request_period) {}

NearbyPeriodicScheduler::~NearbyPeriodicScheduler() = default;

std::optional<base::TimeDelta>
NearbyPeriodicScheduler::TimeUntilRecurringRequest(base::Time now) const {
  std::optional<base::Time> last_success_time = GetLastSuccessTime();

  // Immediately run a first-time request.
  if (!last_success_time) {
    return base::Seconds(0);
  }

  base::TimeDelta time_elapsed_since_last_success = now - *last_success_time;

  return std::max(base::Seconds(0),
                  request_period_ - time_elapsed_since_last_success);
}

}  // namespace ash::nearby
