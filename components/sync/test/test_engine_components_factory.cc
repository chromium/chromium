// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/test_engine_components_factory.h"

#include <memory>

#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "components/sync/test/fake_sync_scheduler.h"

namespace syncer {

std::unique_ptr<SyncScheduler> TestEngineComponentsFactory::BuildScheduler(
    const std::string& name,
    SyncCycleContext* context,
    CancelationSignal* cancelation_signal,
    bool ignore_auth_credentials) {
  return std::unique_ptr<SyncScheduler>(new FakeSyncScheduler());
}

std::unique_ptr<SyncCycleContext> TestEngineComponentsFactory::BuildContext(
    ServerConnectionManager* connection_manager,
    ExtensionsActivity* monitor,
    const std::vector<SyncEngineEventListener*>& listeners,
    DebugInfoGetter* debug_info_getter,
    DataTypeRegistry* data_type_registry,
    const std::string& cache_guid,
    const std::string& store_birthday,
    const std::string& bag_of_chips,
    base::TimeDelta poll_interval) {
  // Tests don't wire up listeners.
  std::vector<SyncEngineEventListener*> empty_listeners;
  return std::make_unique<SyncCycleContext>(
      connection_manager, monitor, empty_listeners, debug_info_getter,
      data_type_registry, cache_guid, store_birthday, bag_of_chips,
      poll_interval);
}

}  // namespace syncer
