// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_TEST_ENGINE_COMPONENTS_FACTORY_H_
#define COMPONENTS_SYNC_ENGINE_TEST_ENGINE_COMPONENTS_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/sync/engine/engine_components_factory.h"

namespace syncer {

class TestEngineComponentsFactory : public EngineComponentsFactory {
 public:
  explicit TestEngineComponentsFactory(StorageOption option,
                                       StorageOption* storage_used);
  ~TestEngineComponentsFactory() override;

  std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* cancelation_signal,
      bool ignore_auth_credentials) override;

  std::unique_ptr<SyncCycleContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      ExtensionsActivity* monitor,
      const std::vector<SyncEngineEventListener*>& listeners,
      DebugInfoGetter* debug_info_getter,
      ModelTypeRegistry* model_type_registry,
      const std::string& invalidator_client_id,
      const std::string& store_birthday,
      const std::string& bag_of_chips,
      base::TimeDelta poll_interval) override;

  std::unique_ptr<syncable::DirectoryBackingStore> BuildDirectoryBackingStore(
      StorageOption storage,
      const std::string& dir_name,
      const base::RepeatingCallback<std::string()>& cache_guid_generator,
      const base::FilePath& backing_filepath) override;

 private:
  const StorageOption storage_override_;
  StorageOption* storage_used_;

  DISALLOW_COPY_AND_ASSIGN(TestEngineComponentsFactory);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_TEST_ENGINE_COMPONENTS_FACTORY_H_
