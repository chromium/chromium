// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_H_
#define COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/sync/engine/model_safe_worker.h"

namespace syncer {

class CancelationSignal;
class DebugInfoGetter;
class ExtensionsActivity;
class ModelTypeRegistry;
class ServerConnectionManager;
class SyncCycleContext;
class SyncEngineEventListener;
class SyncScheduler;

namespace syncable {
class Directory;
class DirectoryBackingStore;
}

// EngineComponentsFactory exists so that tests can override creation of
// components used by the SyncManager and other things inside engine/.
class EngineComponentsFactory {
 public:
  enum EncryptionMethod {
    ENCRYPTION_LEGACY,
    // Option to enable support for keystore key based encryption.
    ENCRYPTION_KEYSTORE
  };

  enum BackoffOverride {
    BACKOFF_NORMAL,
    // Use this value for integration testing to avoid long delays /
    // timing out tests. Uses kInitialBackoffShortRetrySeconds (see
    // polling_constants.h) for all initial retries.
    BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE
  };

  // Configuration options for internal components. This struct is expected
  // to grow and shrink over time with transient features / experiments,
  // roughly following command line flags in chrome. Implementations of
  // EngineComponentsFactory can use this information to build components
  // with appropriate bells and whistles.
  struct Switches {
    EncryptionMethod encryption_method;
    BackoffOverride backoff_override;
    bool force_short_nudge_delay_for_test;
  };

  // For selecting the types of storage to use to persist sync data when
  // BuildDirectoryBackingStore() is called.
  enum StorageOption {
    // BuildDirectoryBackingStore should not use persistent on-disk storage.
    STORAGE_IN_MEMORY,
    // Use this if you want BuildDirectoryBackingStore to create/use a real
    // on disk store.
    STORAGE_ON_DISK,
    // Use this to test the case where a directory fails to load.
    STORAGE_INVALID
  };

  virtual ~EngineComponentsFactory() {}

  virtual std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* cancelation_signal,
      bool ignore_auth_credentials) = 0;

  virtual std::unique_ptr<SyncCycleContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      ExtensionsActivity* extensions_activity,
      const std::vector<SyncEngineEventListener*>& listeners,
      DebugInfoGetter* debug_info_getter,
      ModelTypeRegistry* model_type_registry,
      const std::string& invalidator_client_id,
      const std::string& store_birthday,
      const std::string& bag_of_chips,
      base::TimeDelta poll_interval) = 0;

  virtual std::unique_ptr<syncable::DirectoryBackingStore>
  BuildDirectoryBackingStore(
      StorageOption storage,
      const std::string& dir_name,
      const base::RepeatingCallback<std::string()>& cache_guid_generator,
      const base::FilePath& backing_filepath) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_H_
