// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/service_proxy_impl.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace history {
class HistoryService;
}

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

class PrefService;

namespace segmentation_platform {

namespace proto {
class SegmentInfo;
class SignalData;
class SignalStorageConfigs;
}  // namespace proto

struct Config;
class DatabaseMaintenanceImpl;
class FeatureListQueryProcessor;
class HistogramSignalHandler;
class HistoryServiceObserver;
class ModelExecutionManager;
class ModelExecutionSchedulerImpl;
class ModelProviderFactory;
class SegmentationResultPrefs;
class SegmentInfoDatabase;
class SegmentSelectorImpl;
class SignalDatabaseImpl;
class SignalFilterProcessor;
class SignalStorageConfig;
class SegmentScoreProvider;
class TrainingDataCollector;
class UkmDataManager;
class UserActionSignalHandler;

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

// The internal implementation of the SegmentationPlatformService.
class SegmentationPlatformServiceImpl : public SegmentationPlatformService {
 public:
  SegmentationPlatformServiceImpl(
      std::unique_ptr<ModelProviderFactory> model_provider,
      leveldb_proto::ProtoDatabaseProvider* db_provider,
      const base::FilePath& storage_dir,
      UkmDataManager* ukm_data_manager,
      PrefService* pref_service,
      history::HistoryService* history_service,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      std::vector<std::unique_ptr<Config>> configs);

  // For testing only.
  SegmentationPlatformServiceImpl(
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SegmentInfo>>
          segment_db,
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalData>>
          signal_db,
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalStorageConfigs>>
          signal_storage_config_db,
      UkmDataManager* ukm_data_manager,
      std::unique_ptr<ModelProviderFactory> model_provider,
      PrefService* pref_service,
      history::HistoryService* history_service,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      std::vector<std::unique_ptr<Config>> configs);

  ~SegmentationPlatformServiceImpl() override;

  // Disallow copy/assign.
  SegmentationPlatformServiceImpl(const SegmentationPlatformServiceImpl&) =
      delete;
  SegmentationPlatformServiceImpl& operator=(
      const SegmentationPlatformServiceImpl&) = delete;

  // SegmentationPlatformService overrides.
  void GetSelectedSegment(const std::string& segmentation_key,
                          SegmentSelectionCallback callback) override;
  SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) override;
  void EnableMetrics(bool signal_collection_allowed) override;
  ServiceProxy* GetServiceProxy() override;

 private:
  friend class SegmentationPlatformServiceImplTest;

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

  // Called when service status changes.
  void OnServiceStatusChanged();

  // Moved to ModelExecutionManagerImpl on initialization of service.
  std::unique_ptr<ModelProviderFactory> model_provider_factory_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<base::Clock> clock_;
  const PlatformOptions platform_options_;

  // Config.
  std::vector<std::unique_ptr<Config>> configs_;
  base::flat_set<optimization_guide::proto::OptimizationTarget>
      all_segment_ids_;

  // Databases.
  std::unique_ptr<SegmentInfoDatabase> segment_info_database_;
  std::unique_ptr<SignalDatabaseImpl> signal_database_;
  std::unique_ptr<SignalStorageConfig> signal_storage_config_;
  std::unique_ptr<SegmentationResultPrefs> segmentation_result_prefs_;

  // The data manager is owned by the database client and is guaranteed to be
  // kept alive until all profiles (keyed services) are destroyed. Refer to the
  // description of UkmDataManager to know the lifetime of the objects usable
  // from the manager.
  raw_ptr<UkmDataManager> ukm_data_manager_;

  // Signal processing.
  std::unique_ptr<UserActionSignalHandler> user_action_signal_handler_;
  std::unique_ptr<HistogramSignalHandler> histogram_signal_handler_;
  std::unique_ptr<SignalFilterProcessor> signal_filter_processor_;
  std::unique_ptr<HistoryServiceObserver> history_service_observer_;

  // Training/inference input data generation.
  std::unique_ptr<FeatureListQueryProcessor> feature_list_query_processor_;

  // Segment selection.
  // TODO(shaktisahu): Determine safe destruction ordering between
  // SegmentSelectorImpl and ModelExecutionSchedulerImpl.
  base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>
      segment_selectors_;

  // Segment results.
  std::unique_ptr<SegmentScoreProvider> segment_score_provider_;

  // Traing data collection logic.
  std::unique_ptr<TrainingDataCollector> training_data_collector_;

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

  std::unique_ptr<ServiceProxyImpl> proxy_;

  base::WeakPtrFactory<SegmentationPlatformServiceImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
