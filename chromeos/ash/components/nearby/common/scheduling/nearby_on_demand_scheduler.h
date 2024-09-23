// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_ON_DEMAND_SCHEDULER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_ON_DEMAND_SCHEDULER_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_base.h"

namespace ash::nearby {

// A NearbySchedulerBase that does not schedule recurring tasks.
class NearbyOnDemandScheduler : public NearbySchedulerBase {
 public:
  // See NearbySchedulerBase for a description of inputs.
  NearbyOnDemandScheduler(bool retry_failures,
                          bool require_connectivity,
                          const std::string& pref_name,
                          PrefService* pref_service,
                          OnRequestCallback callback,
                          Feature logging_feature,
                          const base::Clock* clock);

  ~NearbyOnDemandScheduler() override;

 private:
  // Return std::nullopt so as not to schedule recurring requests.
  std::optional<base::TimeDelta> TimeUntilRecurringRequest(
      base::Time now) const override;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_ON_DEMAND_SCHEDULER_H_
