// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/storage_service.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/segmentation_platform/internal/database/database_maintenance_impl.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database_impl.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "components/segmentation_platform/internal/proto/signal_storage_config.pb.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"

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
    base::flat_set<optimization_guide::proto::OptimizationTarget>
        all_segment_ids,
    ModelProviderFactory* model_provider_factory)
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
          clock,
          ukm_data_manager,
          all_segment_ids,
          model_provider_factory) {}

StorageService::StorageService(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SegmentInfo>>
        segment_db,
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalData>> signal_db,
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalStorageConfigs>>
        signal_storage_config_db,
    base::Clock* clock,
    UkmDataManager* ukm_data_manager,
    base::flat_set<optimization_guide::proto::OptimizationTarget>
        all_segment_ids,
    ModelProviderFactory* model_provider_factory)
    : default_model_manager_(std::make_unique<DefaultModelManager>(
          model_provider_factory,
          std::vector<OptimizationTarget>(all_segment_ids.begin(),
                                          all_segment_ids.end()))),
      segment_info_database_(
          std::make_unique<SegmentInfoDatabase>(std::move(segment_db))),
      signal_database_(
          std::make_unique<SignalDatabaseImpl>(std::move(signal_db), clock)),
      signal_storage_config_(std::make_unique<SignalStorageConfig>(
          std::move(signal_storage_config_db),
          clock)),
      ukm_data_manager_(ukm_data_manager),
      database_maintenance_(std::make_unique<DatabaseMaintenanceImpl>(
          all_segment_ids,
          clock,
          segment_info_database_.get(),
          signal_database_.get(),
          signal_storage_config_.get(),
          default_model_manager_.get())) {
  ukm_data_manager_->AddRef();
}

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
  std::move(init_callback_)
      .Run(*segment_info_database_initialized_ &&
           *signal_database_initialized_ &&
           *signal_storage_config_initialized_);

  // Initiate database maintenance tasks with a small delay.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&StorageService::OnExecuteDatabaseMaintenanceTasks,
                     weak_ptr_factory_.GetWeakPtr()),
      kDatabaseMaintenanceDelay);
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

void StorageService::OnExecuteDatabaseMaintenanceTasks() {
  database_maintenance_->ExecuteMaintenanceTasks();
}

}  // namespace segmentation_platform
