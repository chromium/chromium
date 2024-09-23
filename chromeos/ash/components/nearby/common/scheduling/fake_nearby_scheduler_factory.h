// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_FAKE_NEARBY_SCHEDULER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_FAKE_NEARBY_SCHEDULER_FACTORY_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_expiration_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"

class PrefService;

namespace ash::nearby {

class NearbyScheduler;

// A fake NearbyScheduler factory that creates instances of
// FakeNearbyScheduler instead of expiration, on-demand, or periodic
// scheduler. It stores the factory input parameters as well as a raw pointer to
// the fake scheduler for each instance created.
class FakeNearbySchedulerFactory : public NearbySchedulerFactory {
 public:
  struct ExpirationInstance {
    ExpirationInstance();
    ExpirationInstance(ExpirationInstance&&);
    ~ExpirationInstance();

    raw_ptr<FakeNearbyScheduler, DanglingUntriaged> fake_scheduler = nullptr;
    NearbyExpirationScheduler::ExpirationTimeFunctor expiration_time_functor;
    bool retry_failures;
    bool require_connectivity;
    raw_ptr<PrefService, DanglingUntriaged> pref_service = nullptr;
    Feature logging_feature;
    raw_ptr<const base::Clock> clock = nullptr;
  };

  struct OnDemandInstance {
    raw_ptr<FakeNearbyScheduler, DanglingUntriaged> fake_scheduler = nullptr;
    bool retry_failures;
    bool require_connectivity;
    raw_ptr<PrefService, DanglingUntriaged> pref_service = nullptr;
    Feature logging_feature;
    raw_ptr<const base::Clock> clock = nullptr;
  };

  struct PeriodicInstance {
    raw_ptr<FakeNearbyScheduler, DanglingUntriaged> fake_scheduler = nullptr;
    base::TimeDelta request_period;
    bool retry_failures;
    bool require_connectivity;
    raw_ptr<PrefService, DanglingUntriaged> pref_service = nullptr;
    Feature logging_feature;
    raw_ptr<const base::Clock> clock = nullptr;
  };

  FakeNearbySchedulerFactory();
  ~FakeNearbySchedulerFactory() override;

  const std::map<std::string, ExpirationInstance>&
  pref_name_to_expiration_instance() const {
    return pref_name_to_expiration_instance_;
  }

  const std::map<std::string, OnDemandInstance>&
  pref_name_to_on_demand_instance() const {
    return pref_name_to_on_demand_instance_;
  }

  const std::map<std::string, PeriodicInstance>&
  pref_name_to_periodic_instance() const {
    return pref_name_to_periodic_instance_;
  }

 private:
  // NearbySchedulerFactory:
  std::unique_ptr<NearbyScheduler> CreateExpirationSchedulerInstance(
      NearbyExpirationScheduler::ExpirationTimeFunctor expiration_time_functor,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback on_request_callback,
      Feature logging_feature,
      const base::Clock* clock) override;
  std::unique_ptr<NearbyScheduler> CreateOnDemandSchedulerInstance(
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback callback,
      Feature logging_feature,
      const base::Clock* clock) override;
  std::unique_ptr<NearbyScheduler> CreatePeriodicSchedulerInstance(
      base::TimeDelta request_period,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyScheduler::OnRequestCallback callback,
      Feature logging_feature,
      const base::Clock* clock) override;

  std::map<std::string, ExpirationInstance> pref_name_to_expiration_instance_;
  std::map<std::string, OnDemandInstance> pref_name_to_on_demand_instance_;
  std::map<std::string, PeriodicInstance> pref_name_to_periodic_instance_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_FAKE_NEARBY_SCHEDULER_FACTORY_H_
