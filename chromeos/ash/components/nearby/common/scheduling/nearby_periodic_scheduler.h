// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_PERIODIC_SCHEDULER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_PERIODIC_SCHEDULER_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_base.h"

namespace ash::nearby {

// A NearbySchedulerBase that schedules periodic tasks at fixed intervals.
// Immediate requests and/or failure retries can interrupt this pattern. The
// periodic taks is always updated to run a fixed delay after the last
// successful request.
class NearbyPeriodicScheduler : public NearbySchedulerBase {
 public:
  // |request_period|: The fixed delay between periodic requests.
  // See NearbySchedulerBase for a description of other inputs.
  NearbyPeriodicScheduler(base::TimeDelta request_period,
                          bool retry_failures,
                          bool require_connectivity,
                          const std::string& pref_name,
                          PrefService* pref_service,
                          OnRequestCallback callback,
                          Feature logging_feature,
                          const base::Clock* clock);

  ~NearbyPeriodicScheduler() override;

 private:
  // Returns the time until the next periodic request using the time since
  // the last success. Immediately runs a first-time periodic request.
  std::optional<base::TimeDelta> TimeUntilRecurringRequest(
      base::Time now) const override;

  base::TimeDelta request_period_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_PERIODIC_SCHEDULER_H_
