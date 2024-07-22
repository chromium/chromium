// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_API_COMPONENT_FACTORY_IMPL_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_API_COMPONENT_FACTORY_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/sync_api_component_factory.h"

namespace syncer {
class DeviceInfoTracker;
class SyncClient;
class SyncInvalidationsService;
}  // namespace syncer

namespace browser_sync {

class SyncApiComponentFactoryImpl : public syncer::SyncApiComponentFactory {
 public:
  SyncApiComponentFactoryImpl(syncer::SyncClient* sync_client,
                              syncer::DeviceInfoTracker* device_info_tracker,
                              const base::FilePath& sync_data_folder);
  SyncApiComponentFactoryImpl(const SyncApiComponentFactoryImpl&) = delete;
  SyncApiComponentFactoryImpl& operator=(const SyncApiComponentFactoryImpl&) =
      delete;
  ~SyncApiComponentFactoryImpl() override;

  // SyncApiComponentFactory implementation:
  std::unique_ptr<syncer::DataTypeManager> CreateDataTypeManager(
      const syncer::ModelTypeController::TypeMap* controllers,
      const syncer::DataTypeEncryptionHandler* encryption_handler,
      syncer::DataTypeManagerObserver* observer) override;
  std::unique_ptr<syncer::SyncEngine> CreateSyncEngine(
      const std::string& name,
      const signin::GaiaIdHash& gaia_id_hash,
      syncer::SyncInvalidationsService* sync_invalidation_service) override;
  bool HasTransportDataIncludingFirstSync(
      const signin::GaiaIdHash& gaia_id_hash) override;
  void CleanupOnDisableSync() override;
  void ClearTransportDataForAccount(
      const signin::GaiaIdHash& gaia_id_hash) override;

 private:
  const scoped_refptr<base::SequencedTaskRunner>
      engines_and_directory_deletion_thread_;
  const raw_ptr<syncer::SyncClient> sync_client_;
  const raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  const base::FilePath sync_data_folder_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_API_COMPONENT_FACTORY_IMPL_H_
