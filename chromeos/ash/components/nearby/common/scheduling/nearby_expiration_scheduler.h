// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_EXPIRATION_SCHEDULER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_EXPIRATION_SCHEDULER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_base.h"

namespace ash::nearby {

// A NearbySchedulerBase that schedules recurring tasks based on an
// expiration time provided by the owner.
class NearbyExpirationScheduler : public NearbySchedulerBase {
 public:
  using ExpirationTimeFunctor =
      base::RepeatingCallback<std::optional<base::Time>()>;

  // |expiration_time_functor|: A function provided by the owner that returns
  //     the next expiration time.
  // See NearbySchedulerBase for a description of other inputs.
  NearbyExpirationScheduler(ExpirationTimeFunctor expiration_time_functor,
                            bool retry_failures,
                            bool require_connectivity,
                            const std::string& pref_name,
                            PrefService* pref_service,
                            OnRequestCallback on_request_callback,
                            Feature logging_feature,
                            const base::Clock* clock);

  ~NearbyExpirationScheduler() override;

 protected:
  std::optional<base::TimeDelta> TimeUntilRecurringRequest(
      base::Time now) const override;

  ExpirationTimeFunctor expiration_time_functor_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_EXPIRATION_SCHEDULER_H_
