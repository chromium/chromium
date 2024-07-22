// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_api_component_factory_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/browser_sync/active_devices_provider_impl.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/base/legacy_directory_deletion.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/service/data_type_manager_impl.h"
#include "components/sync/service/glue/sync_engine_impl.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_client.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace browser_sync {

using syncer::DataTypeManager;
using syncer::DataTypeManagerImpl;
using syncer::DataTypeManagerObserver;

SyncApiComponentFactoryImpl::SyncApiComponentFactoryImpl(
    syncer::SyncClient* sync_client,
    syncer::DeviceInfoTracker* device_info_tracker,
    const base::FilePath& sync_data_folder)
    : engines_and_directory_deletion_thread_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      sync_client_(sync_client),
      device_info_tracker_(device_info_tracker),
      sync_data_folder_(sync_data_folder) {
  CHECK(sync_client_);
}

SyncApiComponentFactoryImpl::~SyncApiComponentFactoryImpl() = default;

std::unique_ptr<DataTypeManager>
SyncApiComponentFactoryImpl::CreateDataTypeManager(
    const syncer::ModelTypeController::TypeMap* controllers,
    const syncer::DataTypeEncryptionHandler* encryption_handler,
    DataTypeManagerObserver* observer) {
  return std::make_unique<DataTypeManagerImpl>(controllers, encryption_handler,
                                               observer);
}

std::unique_ptr<syncer::SyncEngine>
SyncApiComponentFactoryImpl::CreateSyncEngine(
    const std::string& name,
    const signin::GaiaIdHash& gaia_id_hash,
    syncer::SyncInvalidationsService* sync_invalidation_service) {
  return std::make_unique<syncer::SyncEngineImpl>(
      name, sync_invalidation_service,
      std::make_unique<browser_sync::ActiveDevicesProviderImpl>(
          device_info_tracker_, base::DefaultClock::GetInstance()),
      std::make_unique<syncer::SyncTransportDataPrefs>(
          sync_client_->GetPrefService(), gaia_id_hash),
      sync_data_folder_, engines_and_directory_deletion_thread_);
}

bool SyncApiComponentFactoryImpl::HasTransportDataIncludingFirstSync(
    const signin::GaiaIdHash& gaia_id_hash) {
  syncer::SyncTransportDataPrefs sync_transport_data_prefs(
      sync_client_->GetPrefService(), gaia_id_hash);
  // NOTE: Keep this logic consistent with how SyncEngineImpl reports
  // is-first-sync.
  return !sync_transport_data_prefs.GetLastSyncedTime().is_null();
}

void SyncApiComponentFactoryImpl::CleanupOnDisableSync() {
  PrefService* pref_service = sync_client_->GetPrefService();
  // Clearing the Directory via DeleteLegacyDirectoryFilesAndNigoriStorage()
  // means there's IO involved which may be considerable overhead if
  // triggered consistently upon browser startup (which is the case for
  // certain codepaths such as the user being signed out). To avoid that, prefs
  // are used to determine whether it's worth it.
  if (syncer::SyncTransportDataPrefs::HasCurrentSyncingGaiaId(pref_service)) {
    syncer::SyncTransportDataPrefs::ClearCurrentSyncingGaiaId(pref_service);
    engines_and_directory_deletion_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&syncer::DeleteLegacyDirectoryFilesAndNigoriStorage,
                       sync_data_folder_));
  }

  syncer::SyncTransportDataPrefs::ClearAllLegacy(pref_service);
}

void SyncApiComponentFactoryImpl::ClearTransportDataForAccount(
    const signin::GaiaIdHash& gaia_id_hash) {
  syncer::SyncTransportDataPrefs prefs(sync_client_->GetPrefService(),
                                       gaia_id_hash);
  prefs.ClearForCurrentAccount();
}

}  // namespace browser_sync
