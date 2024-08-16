// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/engine_components_factory_impl.h"

#include <map>
#include <utility>

#include "components/sync/engine/backoff_delay_provider.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "components/sync/engine/sync_scheduler_impl.h"
#include "components/sync/engine/syncer.h"

namespace syncer {

EngineComponentsFactoryImpl::EngineComponentsFactoryImpl(
    const Switches& switches)
    : switches_(switches) {}

std::unique_ptr<SyncScheduler> EngineComponentsFactoryImpl::BuildScheduler(
    const std::string& name,
    SyncCycleContext* context,
    CancelationSignal* cancelation_signal,
    bool ignore_auth_credentials) {
  std::unique_ptr<BackoffDelayProvider> delay =
      (switches_.backoff_override == BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE)
          ? BackoffDelayProvider::WithShortInitialRetryOverride()
          : BackoffDelayProvider::FromDefaults();

  std::unique_ptr<SyncSchedulerImpl> scheduler =
      std::make_unique<SyncSchedulerImpl>(
          name, std::move(delay), context,
          std::make_unique<Syncer>(cancelation_signal),
          ignore_auth_credentials);
  if (switches_.force_short_nudge_delay_for_test) {
    scheduler->ForceShortNudgeDelayForTest();
  }
  return std::move(scheduler);
}

std::unique_ptr<SyncCycleContext> EngineComponentsFactoryImpl::BuildContext(
    ServerConnectionManager* connection_manager,
    ExtensionsActivity* extensions_activity,
    const std::vector<SyncEngineEventListener*>& listeners,
    DebugInfoGetter* debug_info_getter,
    DataTypeRegistry* data_type_registry,
    const std::string& cache_guid,
    const std::string& store_birthday,
    const std::string& bag_of_chips,
    base::TimeDelta poll_interval) {
  return std::make_unique<SyncCycleContext>(
      connection_manager, extensions_activity, listeners, debug_info_getter,
      data_type_registry, cache_guid, store_birthday, bag_of_chips,
      poll_interval);
}

}  // namespace syncer
