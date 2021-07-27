// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_

#include "components/segmentation_platform/public/segmentation_platform_service.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace base {
class Clock;
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

class PrefService;

namespace segmentation_platform {

namespace proto {
class SegmentInfo;
class SignalData;
class SignalStorageConfigs;
}  // namespace proto

struct Config;
class DatabaseMaintenanceImpl;
class HistogramSignalHandler;
class ModelExecutionManager;
class ModelExecutionSchedulerImpl;
class SegmentationResultPrefs;
class SegmentInfoDatabase;
class SegmentSelectorImpl;
class SignalDatabaseImpl;
class SignalFilterProcessor;
class SignalStorageConfig;
class UserActionSignalHandler;

// The internal implementation of the SegmentationPlatformService.
class SegmentationPlatformServiceImpl : public SegmentationPlatformService {
 public:
  SegmentationPlatformServiceImpl(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      leveldb_proto::ProtoDatabaseProvider* db_provider,
      const base::FilePath& storage_dir,
      PrefService* pref_service,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      std::unique_ptr<Config> config);

  // For testing only.
  SegmentationPlatformServiceImpl(
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SegmentInfo>>
          segment_db,
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalData>>
          signal_db,
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalStorageConfigs>>
          signal_storage_config_db,
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      PrefService* pref_service,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      std::unique_ptr<Config> config);

  ~SegmentationPlatformServiceImpl() override;

  // Disallow copy/assign.
  SegmentationPlatformServiceImpl(const SegmentationPlatformServiceImpl&) =
      delete;
  SegmentationPlatformServiceImpl& operator=(
      const SegmentationPlatformServiceImpl&) = delete;

  // SegmentationPlatformService overrides.
  void GetSelectedSegment(const std::string& segmentation_key,
                          SegmentSelectionCallback callback) override;
  void EnableMetrics(bool signal_collection_allowed) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SegmentationPlatformServiceImplTest,
                           InitializationFlow);

  void OnSegmentInfoDatabaseInitialized(bool success);
  void OnSignalDatabaseInitialized(bool success);
  void OnSignalStorageConfigInitialized(bool success);
  bool IsInitializationFinished() const;
  void MaybeRunPostInitializationRoutines();
  // Must only be invoked with a valid SegmentInfo.
  void OnSegmentationModelUpdated(proto::SegmentInfo segment_info);

  // Executes all database maintenance tasks. This should be invoked after a
  // short amount of time has passed since initialization happened.
  void OnExecuteDatabaseMaintenanceTasks();

  optimization_guide::OptimizationGuideModelProvider* model_provider_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::Clock* clock_;

  // Config.
  std::unique_ptr<Config> config_;

  // Databases.
  std::unique_ptr<SegmentInfoDatabase> segment_info_database_;
  std::unique_ptr<SignalDatabaseImpl> signal_database_;
  std::unique_ptr<SignalStorageConfig> signal_storage_config_;
  std::unique_ptr<SegmentationResultPrefs> segmentation_result_prefs_;

  // Signal processing.
  std::unique_ptr<UserActionSignalHandler> user_action_signal_handler_;
  std::unique_ptr<HistogramSignalHandler> histogram_signal_handler_;
  std::unique_ptr<SignalFilterProcessor> signal_filter_processor_;

  // Segment selection.
  // TODO(shaktisahu): Determine safe destruction ordering between
  // SegmentSelectorImpl and ModelExecutionSchedulerImpl.
  std::unique_ptr<SegmentSelectorImpl> segment_selector_;

  // Model execution scheduling logic.
  std::unique_ptr<ModelExecutionSchedulerImpl> model_execution_scheduler_;

  // Model execution.
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;

  // Database maintenance.
  std::unique_ptr<DatabaseMaintenanceImpl> database_maintenance_;

  // Database initialization statuses.
  absl::optional<bool> segment_info_database_initialized_;
  absl::optional<bool> signal_database_initialized_;
  absl::optional<bool> signal_storage_config_initialized_;

  base::WeakPtrFactory<SegmentationPlatformServiceImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
