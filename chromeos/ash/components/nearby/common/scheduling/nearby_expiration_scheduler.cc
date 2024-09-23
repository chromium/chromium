// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/nearby_expiration_scheduler.h"

#include <utility>

namespace ash::nearby {

NearbyExpirationScheduler::NearbyExpirationScheduler(
    ExpirationTimeFunctor expiration_time_functor,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    OnRequestCallback on_request_callback,
    Feature logging_feature,
    const base::Clock* clock)
    : NearbySchedulerBase(retry_failures,
                          require_connectivity,
                          pref_name,
                          pref_service,
                          std::move(on_request_callback),
                          logging_feature,
                          clock),
      expiration_time_functor_(std::move(expiration_time_functor)) {}

NearbyExpirationScheduler::~NearbyExpirationScheduler() = default;

std::optional<base::TimeDelta>
NearbyExpirationScheduler::TimeUntilRecurringRequest(base::Time now) const {
  std::optional<base::Time> expiration_time = expiration_time_functor_.Run();
  if (!expiration_time) {
    return std::nullopt;
  }

  if (*expiration_time <= now) {
    return base::Seconds(0);
  }

  return *expiration_time - now;
}

}  // namespace ash::nearby
