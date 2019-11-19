// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/engine_components_factory_impl.h"

#include <map>
#include <utility>

#include "components/sync/engine_impl/backoff_delay_provider.h"
#include "components/sync/engine_impl/cycle/sync_cycle_context.h"
#include "components/sync/engine_impl/sync_scheduler_impl.h"
#include "components/sync/engine_impl/syncer.h"
#include "components/sync/syncable/on_disk_directory_backing_store.h"

namespace syncer {

EngineComponentsFactoryImpl::EngineComponentsFactoryImpl(
    const Switches& switches)
    : switches_(switches) {}

EngineComponentsFactoryImpl::~EngineComponentsFactoryImpl() {}

std::unique_ptr<SyncScheduler> EngineComponentsFactoryImpl::BuildScheduler(
    const std::string& name,
    SyncCycleContext* context,
    CancelationSignal* cancelation_signal,
    bool ignore_auth_credentials) {
  std::unique_ptr<BackoffDelayProvider> delay(
      BackoffDelayProvider::FromDefaults());

  if (switches_.backoff_override == BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE) {
    delay.reset(BackoffDelayProvider::WithShortInitialRetryOverride());
  }

  std::unique_ptr<SyncSchedulerImpl> scheduler =
      std::make_unique<SyncSchedulerImpl>(name, delay.release(), context,
                                          new Syncer(cancelation_signal),
                                          ignore_auth_credentials);
  if (switches_.force_short_nudge_delay_for_test) {
    scheduler->ForceShortNudgeDelayForTest();
  }
  return std::move(scheduler);
}

std::unique_ptr<SyncCycleContext> EngineComponentsFactoryImpl::BuildContext(
    ServerConnectionManager* connection_manager,
    syncable::Directory* directory,
    ExtensionsActivity* extensions_activity,
    const std::vector<SyncEngineEventListener*>& listeners,
    DebugInfoGetter* debug_info_getter,
    ModelTypeRegistry* model_type_registry,
    const std::string& invalidation_client_id,
    const std::string& store_birthday,
    const std::string& bag_of_chips,
    base::TimeDelta poll_interval) {
  return std::make_unique<SyncCycleContext>(
      connection_manager, directory, extensions_activity, listeners,
      debug_info_getter, model_type_registry, invalidation_client_id,
      store_birthday, bag_of_chips, poll_interval);
}

std::unique_ptr<syncable::DirectoryBackingStore>
EngineComponentsFactoryImpl::BuildDirectoryBackingStore(
    StorageOption storage,
    const std::string& dir_name,
    const base::RepeatingCallback<std::string()>& cache_guid_generator,
    const base::FilePath& backing_filepath) {
  if (storage == STORAGE_ON_DISK) {
    return std::unique_ptr<syncable::DirectoryBackingStore>(
        new syncable::OnDiskDirectoryBackingStore(
            dir_name, cache_guid_generator, backing_filepath));
  } else {
    NOTREACHED();
    return std::unique_ptr<syncable::DirectoryBackingStore>();
  }
}

}  // namespace syncer
