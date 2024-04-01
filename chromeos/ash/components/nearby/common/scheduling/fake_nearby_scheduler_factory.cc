// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler_factory.h"

#include <utility>

namespace ash::nearby {

FakeNearbySchedulerFactory::ExpirationInstance::ExpirationInstance() = default;

FakeNearbySchedulerFactory::ExpirationInstance::ExpirationInstance(
    ExpirationInstance&&) = default;

FakeNearbySchedulerFactory::ExpirationInstance::~ExpirationInstance() = default;

FakeNearbySchedulerFactory::FakeNearbySchedulerFactory() = default;

FakeNearbySchedulerFactory::~FakeNearbySchedulerFactory() = default;

std::unique_ptr<NearbyScheduler>
FakeNearbySchedulerFactory::CreateExpirationSchedulerInstance(
    NearbyExpirationScheduler::ExpirationTimeFunctor expiration_time_functor,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyScheduler::OnRequestCallback on_request_callback,
    Feature logging_feature,
    const base::Clock* clock) {
  ExpirationInstance instance;
  instance.expiration_time_functor = std::move(expiration_time_functor);
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;
  instance.pref_service = pref_service;
  instance.logging_feature = logging_feature;
  instance.clock = clock;

  auto scheduler =
      std::make_unique<FakeNearbyScheduler>(std::move(on_request_callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_expiration_instance_.erase(pref_name);
  pref_name_to_expiration_instance_.emplace(pref_name, std::move(instance));

  return scheduler;
}

std::unique_ptr<NearbyScheduler>
FakeNearbySchedulerFactory::CreateOnDemandSchedulerInstance(
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyScheduler::OnRequestCallback callback,
    Feature logging_feature,
    const base::Clock* clock) {
  OnDemandInstance instance;
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;
  instance.pref_service = pref_service;
  instance.logging_feature = logging_feature;
  instance.clock = clock;

  auto scheduler = std::make_unique<FakeNearbyScheduler>(std::move(callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_on_demand_instance_.erase(pref_name);
  pref_name_to_on_demand_instance_.emplace(pref_name, instance);

  return scheduler;
}

std::unique_ptr<NearbyScheduler>
FakeNearbySchedulerFactory::CreatePeriodicSchedulerInstance(
    base::TimeDelta request_period,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyScheduler::OnRequestCallback callback,
    Feature logging_feature,
    const base::Clock* clock) {
  PeriodicInstance instance;
  instance.request_period = request_period;
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;
  instance.pref_service = pref_service;
  instance.logging_feature = logging_feature;
  instance.clock = clock;

  auto scheduler = std::make_unique<FakeNearbyScheduler>(std::move(callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_periodic_instance_.erase(pref_name);
  pref_name_to_periodic_instance_.emplace(pref_name, instance);

  return scheduler;
}

}  // namespace ash::nearby
