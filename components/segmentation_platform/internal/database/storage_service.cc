// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/storage_service.h"

#include "base/task/sequenced_task_runner.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/database/database_maintenance_impl.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database_impl.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "components/segmentation_platform/internal/proto/signal_storage_config.pb.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform {
namespace {
const base::FilePath::CharType kSegmentInfoDBName[] =
    FILE_PATH_LITERAL("SegmentInfoDB");
const base::FilePath::CharType kSignalDBName[] = FILE_PATH_LITERAL("SignalDB");
const base::FilePath::CharType kSignalStorageConfigDBName[] =
    FILE_PATH_LITERAL("SignalStorageConfigDB");
const base::TimeDelta kDatabaseMaintenanceDelay = base::Seconds(30);
}  // namespace

StorageService::StorageService(
    const base::FilePath& storage_dir,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Clock* clock,
    UkmDataManager* ukm_data_manager,
    std::vector<std::unique_ptr<Config>> configs,
    ModelProviderFactory* model_provider_factory,
    PrefService* profile_prefs,
    const std::string& profile_id,
    ModelManager::SegmentationModelUpdatedCallback model_updated_callback)
    : StorageService(
          db_provider->GetDB<proto::SegmentInfo>(
              leveldb_proto::ProtoDbType::SEGMENT_INFO_DATABASE,
              storage_dir.Append(kSegmentInfoDBName),
              task_runner),
          db_provider->GetDB<proto::SignalData>(
              leveldb_proto::ProtoDbType::SIGNAL_DATABASE,
              storage_dir.Append(kSignalDBName),
              task_runner),
          db_provider->GetDB<proto::SignalStorageConfigs>(
              leveldb_proto::ProtoDbType::SIGNAL_STORAGE_CONFIG_DATABASE,
              storage_dir.Append(kSignalStorageConfigDBName),
              task_runner),
          task_runner,
          clock,
          ukm_data_manager,
          std::move(configs),
          model_provider_factory,
          profile_prefs,
          profile_id,
          model_updated_callback) {}

StorageService::StorageService(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SegmentInfo>>
        segment_db,
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalData>> signal_db,
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalStorageConfigs>>
        signal_storage_config_db,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Clock* clock,
    UkmDataManager* ukm_data_manager,
    std::vector<std::unique_ptr<Config>> configs,
    ModelProviderFactory* model_provider_factory,
    PrefService* profile_prefs,
    const std::string& profile_id,
    ModelManager::SegmentationModelUpdatedCallback model_updated_callback)
    : config_holder_(std::make_unique<ConfigHolder>(std::move(configs))),
      client_result_prefs_(std::make_unique<ClientResultPrefs>(profile_prefs)),
      cached_result_provider_(
          std::make_unique<CachedResultProvider>(client_result_prefs_.get(),
                                                 config_holder_->configs())),
      cached_result_writer_(
          std::make_unique<CachedResultWriter>(client_result_prefs_.get(),
                                               clock)),
      segment_info_database_(std::make_unique<SegmentInfoDatabase>(
          std::move(segment_db),
          std::make_unique<SegmentInfoCache>())),
      signal_database_(
          std::make_unique<SignalDatabaseImpl>(std::move(signal_db),
                                               clock,
                                               task_runner)),
      signal_storage_config_(std::make_unique<SignalStorageConfig>(
          std::move(signal_storage_config_db),
          clock)),
      model_manager_(
          std::make_unique<ModelManagerImpl>(config_holder_->all_segment_ids(),
                                             model_provider_factory,
                                             clock,
                                             segment_info_database_.get(),
                                             model_updated_callback)),
      ukm_data_manager_(ukm_data_manager),
      profile_id_(profile_id),
      database_maintenance_(std::make_unique<DatabaseMaintenanceImpl>(
          config_holder_->all_segment_ids(),
          clock,
          this,
          profile_prefs)) {
  ukm_data_manager_->AddRef();
}

StorageService::StorageService(
    std::unique_ptr<SegmentInfoDatabase> segment_info_database,
    std::unique_ptr<SignalDatabase> signal_database,
    std::unique_ptr<SignalStorageConfig> signal_storage_config,
    std::unique_ptr<ModelManager> model_manager,
    std::unique_ptr<ConfigHolder> config_holder,
    UkmDataManager* ukm_data_manager)
    : config_holder_(std::move(config_holder)),
      segment_info_database_(std::move(segment_info_database)),
      signal_database_(std::move(signal_database)),
      signal_storage_config_(std::move(signal_storage_config)),
      model_manager_(std::move(model_manager)),
      ukm_data_manager_(ukm_data_manager) {}

StorageService::~StorageService() {
  ukm_data_manager_->RemoveRef();
}

void StorageService::Initialize(SuccessCallback callback) {
  DCHECK(init_callback_.is_null());
  init_callback_ = std::move(callback);
  segment_info_database_->Initialize(
      base::BindOnce(&StorageService::OnSegmentInfoDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
  signal_database_->Initialize(
      base::BindOnce(&StorageService::OnSignalDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
  signal_storage_config_->InitAndLoad(
      base::BindOnce(&StorageService::OnSignalStorageConfigInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StorageService::OnSegmentInfoDatabaseInitialized(bool success) {
  segment_info_database_initialized_ = success;
  MaybeFinishInitialization();
}

void StorageService::OnSignalDatabaseInitialized(bool success) {
  signal_database_initialized_ = success;
  MaybeFinishInitialization();
}

void StorageService::OnSignalStorageConfigInitialized(bool success) {
  signal_storage_config_initialized_ = success;
  MaybeFinishInitialization();
}

bool StorageService::IsInitializationFinished() const {
  return segment_info_database_initialized_.has_value() &&
         signal_database_initialized_.has_value() &&
         signal_storage_config_initialized_.has_value();
}

void StorageService::MaybeFinishInitialization() {
  if (!IsInitializationFinished())
    return;
  bool init_success = *segment_info_database_initialized_ &&
                      *signal_database_initialized_ &&
                      *signal_storage_config_initialized_;
  if (init_success) {
    model_manager_->Initialize();
  }
  std::move(init_callback_).Run(init_success);
}

int StorageService::GetServiceStatus() const {
  int status = static_cast<int>(ServiceStatus::kUninitialized);
  if (segment_info_database_initialized_)
    status |= static_cast<int>(ServiceStatus::kSegmentationInfoDbInitialized);
  if (signal_database_initialized_)
    status |= static_cast<int>(ServiceStatus::kSignalDbInitialized);
  if (signal_storage_config_initialized_) {
    status |= static_cast<int>(ServiceStatus::kSignalStorageConfigInitialized);
  }
  return status;
}

void StorageService::ExecuteDatabaseMaintenanceTasks(bool is_startup) {
  if (is_startup) {
    // Initiate database maintenance tasks with a small delay at startup.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&StorageService::ExecuteDatabaseMaintenanceTasks,
                       weak_ptr_factory_.GetWeakPtr(), false),
        kDatabaseMaintenanceDelay);
    return;
  }

  // This should be invoked at least after a short amount of time has passed
  // since initialization happened.
  database_maintenance_->ExecuteMaintenanceTasks();
}

base::WeakPtr<StorageService> StorageService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace segmentation_platform
