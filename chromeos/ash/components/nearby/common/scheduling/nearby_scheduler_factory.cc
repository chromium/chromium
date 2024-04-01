// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"

#include <utility>

#include "chromeos/ash/components/nearby/common/scheduling/nearby_on_demand_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_periodic_scheduler.h"

namespace ash::nearby {

// static
NearbySchedulerFactory* NearbySchedulerFactory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyScheduler>
NearbySchedulerFactory::CreateExpirationScheduler(
    NearbyExpirationScheduler::ExpirationTimeFunctor expiration_time_functor,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyScheduler::OnRequestCallback on_request_callback,
    Feature logging_feature,
    const base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreateExpirationSchedulerInstance(
        std::move(expiration_time_functor), retry_failures,
        require_connectivity, pref_name, pref_service,
        std::move(on_request_callback), logging_feature, clock);
  }

  return std::make_unique<NearbyExpirationScheduler>(
      std::move(expiration_time_functor), retry_failures, require_connectivity,
      pref_name, pref_service, std::move(on_request_callback), logging_feature,
      clock);
}

// static
std::unique_ptr<NearbyScheduler>
NearbySchedulerFactory::CreateOnDemandScheduler(
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyScheduler::OnRequestCallback callback,
    Feature logging_feature,
    const base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreateOnDemandSchedulerInstance(
        retry_failures, require_connectivity, pref_name, pref_service,
        std::move(callback), logging_feature, clock);
  }

  return std::make_unique<NearbyOnDemandScheduler>(
      retry_failures, require_connectivity, pref_name, pref_service,
      std::move(callback), logging_feature, clock);
}

// static
std::unique_ptr<NearbyScheduler>
NearbySchedulerFactory::CreatePeriodicScheduler(
    base::TimeDelta request_period,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyScheduler::OnRequestCallback callback,
    Feature logging_feature,
    const base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreatePeriodicSchedulerInstance(
        request_period, retry_failures, require_connectivity, pref_name,
        pref_service, std::move(callback), logging_feature, clock);
  }

  return std::make_unique<NearbyPeriodicScheduler>(
      request_period, retry_failures, require_connectivity, pref_name,
      pref_service, std::move(callback), logging_feature, clock);
}

// static
void NearbySchedulerFactory::SetFactoryForTesting(
    NearbySchedulerFactory* test_factory) {
  test_factory_ = test_factory;
}

NearbySchedulerFactory::~NearbySchedulerFactory() = default;

}  // namespace ash::nearby
