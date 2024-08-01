// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_STORAGE_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_STORAGE_SERVICE_H_

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/internal/database/cached_result_writer.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/execution/model_manager.h"
#include "components/segmentation_platform/internal/execution/model_manager_impl.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

class PrefService;

namespace base {
class Clock;
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace segmentation_platform {

namespace proto {
class SegmentInfo;
class SignalData;
class SignalStorageConfigs;
}  // namespace proto

class DatabaseMaintenanceImpl;
class ModelManager;
class ModelProviderFactory;
class SegmentInfoDatabase;
class SignalDatabase;
class SignalStorageConfig;
class UkmDataManager;

// Qualifiers used to indicate service status. One or more qualifiers can
// be used at a time.
enum class ServiceStatus {
  // Server not yet initialized.
  kUninitialized = 0,

  // Segmentation information DB is initialized.
  kSegmentationInfoDbInitialized = 1,

  // Signal database is initialized.
  kSignalDbInitialized = 1 << 1,

  // Signal storage config is initialized.
  kSignalStorageConfigInitialized = 1 << 2,
};

// Owns and manages all the storage databases for the platform.
class StorageService {
 public:
  StorageService(
      const base::FilePath& storage_dir,
      leveldb_proto::ProtoDatabaseProvider* db_provider,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Clock* clock,
      UkmDataManager* ukm_data_manager,
      std::vector<std::unique_ptr<Config>> configs,
      ModelProviderFactory* model_provider_factory,
      PrefService* profile_prefs,
      const std::string& profile_id,
      ModelManager::SegmentationModelUpdatedCallback model_updated_callback);

  // For tests:
  StorageService(
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SegmentInfo>>
          segment_db,
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalData>>
          signal_db,
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalStorageConfigs>>
          signal_storage_config_db,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Clock* clock,
      UkmDataManager* ukm_data_manager,
      std::vector<std::unique_ptr<Config>> configs,
      ModelProviderFactory* model_provider_factory,
      PrefService* profile_prefs,
      const std::string& profile_id,
      ModelManager::SegmentationModelUpdatedCallback model_updated_callback);

  // For tests:
  StorageService(std::unique_ptr<SegmentInfoDatabase> segment_info_database,
                 std::unique_ptr<SignalDatabase> signal_database,
                 std::unique_ptr<SignalStorageConfig> signal_storage_config,
                 std::unique_ptr<ModelManager> model_manager,
                 std::unique_ptr<ConfigHolder> config_holder,
                 UkmDataManager* ukm_data_manager);

  ~StorageService();

  StorageService(const StorageService&) = delete;
  StorageService& operator=(const StorageService&) = delete;

  // Initialize all the databases and returns true when all of them are
  // initialized successfully.
  using SuccessCallback = base::OnceCallback<void(bool)>;
  void Initialize(SuccessCallback callback);

  // Returns a bitmap of the service status. See `ServiceStatus` enum for the
  // bitmap values.
  int GetServiceStatus() const;

  // Executes all database maintenance tasks.
  void ExecuteDatabaseMaintenanceTasks(bool is_startup);

  const ConfigHolder* config_holder() const { return config_holder_.get(); }

  CachedResultProvider* cached_result_provider() {
    return cached_result_provider_.get();
  }

  CachedResultWriter* cached_result_writer() {
    return cached_result_writer_.get();
  }

  ModelManager* model_manager() {
    DCHECK(model_manager_);
    return model_manager_.get();
  }

  SegmentInfoDatabase* segment_info_database() {
    return segment_info_database_.get();
  }

  SignalDatabase* signal_database() { return signal_database_.get(); }

  SignalStorageConfig* signal_storage_config() {
    return signal_storage_config_.get();
  }

  UkmDataManager* ukm_data_manager() { return ukm_data_manager_; }

  const std::string& profile_id() const { return profile_id_; }

  ClientResultPrefs* client_result_prefs() {
    return client_result_prefs_.get();
  }

  void set_cached_result_writer_for_testing(
      std::unique_ptr<CachedResultWriter> writer) {
    cached_result_writer_ = std::move(writer);
  }
  void set_cached_result_provider_for_testing(
      std::unique_ptr<CachedResultProvider> provider) {
    cached_result_provider_ = std::move(provider);
  }
  void set_profile_id_for_testing(const std::string& profile_id) {
    profile_id_ = profile_id;
  }

  // Get a WeakPtr to the service. Feature processors are destroyed after
  // service sometimes due to posted tasks. WeakPtr is useful to refer to the
  // service.
  base::WeakPtr<StorageService> GetWeakPtr();

 private:
  void OnSegmentInfoDatabaseInitialized(bool success);
  void OnSignalDatabaseInitialized(bool success);
  void OnSignalStorageConfigInitialized(bool success);
  bool IsInitializationFinished() const;
  void MaybeFinishInitialization();

  // All client Configs.
  std::unique_ptr<ConfigHolder> config_holder_;

  std::unique_ptr<ClientResultPrefs> client_result_prefs_;

  // Result cache.
  std::unique_ptr<CachedResultProvider> cached_result_provider_;

  // Writes to result cache.
  std::unique_ptr<CachedResultWriter> cached_result_writer_;

  // Databases.
  std::unique_ptr<SegmentInfoDatabase> segment_info_database_;
  std::unique_ptr<SignalDatabase> signal_database_;
  std::unique_ptr<SignalStorageConfig> signal_storage_config_;

  // Provides provider for default and server models.
  std::unique_ptr<ModelManager> model_manager_;

  // The data manager is owned by the database client and is guaranteed to be
  // kept alive until all profiles (keyed services) are destroyed. Refer to the
  // description of UkmDataManager to know the lifetime of the objects usable
  // from the manager.
  raw_ptr<UkmDataManager> ukm_data_manager_;

  // The profile ID of the current profile, used to query the UKM database.
  std::string profile_id_;

  // Database maintenance.
  std::unique_ptr<DatabaseMaintenanceImpl> database_maintenance_;

  // Database initialization statuses.
  std::optional<bool> segment_info_database_initialized_;
  std::optional<bool> signal_database_initialized_;
  std::optional<bool> signal_storage_config_initialized_;
  SuccessCallback init_callback_;

  base::WeakPtrFactory<StorageService> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_STORAGE_SERVICE_H_
