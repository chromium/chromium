// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_SCHEDULER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_SCHEDULER_FACTORY_H_

#include <memory>
#include <string>

#include "base/time/default_clock.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_expiration_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler.h"

class PrefService;

namespace ash::nearby {

class NearbyScheduler;

// Used to create instances of NearbyExpirationScheduler,
// NearbyOnDemandScheduler, and NearbyPeriodicScheduler. A fake
// factory can also be set for testing purposes.
class NearbySchedulerFactory {
 public:
  static std::unique_ptr<NearbyScheduler> CreateExpirationScheduler(
      NearbyExpirationScheduler::ExpirationTimeFunctor expiration_time_functor,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback on_request_callback,
      Feature logging_feature,
      const base::Clock* clock = base::DefaultClock::GetInstance());

  static std::unique_ptr<NearbyScheduler> CreateOnDemandScheduler(
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback callback,
      Feature logging_feature,
      const base::Clock* clock = base::DefaultClock::GetInstance());

  static std::unique_ptr<NearbyScheduler> CreatePeriodicScheduler(
      base::TimeDelta request_period,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback callback,
      Feature logging_feature,
      const base::Clock* clock = base::DefaultClock::GetInstance());

  static void SetFactoryForTesting(NearbySchedulerFactory* test_factory);

 protected:
  virtual ~NearbySchedulerFactory();

  virtual std::unique_ptr<NearbyScheduler> CreateExpirationSchedulerInstance(
      NearbyExpirationScheduler::ExpirationTimeFunctor expiration_time_functor,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback on_request_callback,
      Feature logging_feature,
      const base::Clock* clock) = 0;

  virtual std::unique_ptr<NearbyScheduler> CreateOnDemandSchedulerInstance(
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback callback,
      Feature logging_feature,
      const base::Clock* clock) = 0;

  virtual std::unique_ptr<NearbyScheduler> CreatePeriodicSchedulerInstance(
      base::TimeDelta request_period,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback callback,
      Feature logging_feature,
      const base::Clock* clock) = 0;

 private:
  static NearbySchedulerFactory* test_factory_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_SCHEDULER_FACTORY_H_
