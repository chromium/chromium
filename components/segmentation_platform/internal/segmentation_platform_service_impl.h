// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database_client_impl.h"
#include "components/segmentation_platform/internal/metrics/field_trial_recorder.h"
#include "components/segmentation_platform/internal/migration/prefs_migrator.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/selection/result_refresh_manager.h"
#include "components/segmentation_platform/internal/service_proxy_impl.h"
#include "components/segmentation_platform/internal/signals/signal_handler.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sync_device_info/device_info_tracker.h"

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

namespace processing {
class InputDelegateHolder;
}

struct Config;
class RequestDispatcher;
class FieldTrialRegister;
class ModelProviderFactory;
class SegmentSelectorImpl;
class SegmentScoreProvider;
class UkmDataManager;

// The internal implementation of the SegmentationPlatformService.
class SegmentationPlatformServiceImpl : public SegmentationPlatformService {
 public:
  struct InitParams {
    InitParams();
    ~InitParams();

    bool IsValid();

    // Profile data:
    std::string profile_id;
    raw_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider = nullptr;
    raw_ptr<history::HistoryService> history_service = nullptr;
    base::FilePath storage_dir;
    raw_ptr<PrefService> profile_prefs = nullptr;

    // Device info tracker. It is owned by
    // the DeviceInfoSynceService, which the segmentation platform service
    // depends on. It is guaranteed to outlive the segmentation platform
    // service.
    raw_ptr<syncer::DeviceInfoTracker> device_info_tracker = nullptr;

    // Platform configuration:
    std::unique_ptr<ModelProviderFactory> model_provider;
    raw_ptr<UkmDataManager> ukm_data_manager = nullptr;
    std::vector<std::unique_ptr<Config>> configs;
    std::unique_ptr<FieldTrialRegister> field_trial_register;
    std::unique_ptr<processing::InputDelegateHolder> input_delegate_holder;

    scoped_refptr<base::SequencedTaskRunner> task_runner;
    raw_ptr<base::Clock> clock = nullptr;

    // Test only:
    std::unique_ptr<StorageService> storage_service;
  };

  explicit SegmentationPlatformServiceImpl(
      std::unique_ptr<InitParams> init_params);

  SegmentationPlatformServiceImpl();

  ~SegmentationPlatformServiceImpl() override;

  // Disallow copy/assign.
  SegmentationPlatformServiceImpl(const SegmentationPlatformServiceImpl&) =
      delete;
  SegmentationPlatformServiceImpl& operator=(
      const SegmentationPlatformServiceImpl&) = delete;

  // SegmentationPlatformService overrides.
  void GetSelectedSegment(const std::string& segmentation_key,
                          SegmentSelectionCallback callback) override;
  void GetClassificationResult(const std::string& segmentation_key,
                               const PredictionOptions& prediction_options,
                               scoped_refptr<InputContext> input_context,
                               ClassificationResultCallback callback) override;
  void GetAnnotatedNumericResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      AnnotatedNumericResultCallback callback) override;
  SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) override;
  void CollectTrainingData(SegmentId segment_id,
                           TrainingRequestId request_id,
                           const TrainingLabels& param,
                           SuccessCallback callback) override;
  void CollectTrainingData(SegmentId segment_id,
                           TrainingRequestId request_id,
                           ukm::SourceId ukm_source_id,
                           const TrainingLabels& param,
                           SuccessCallback callback) override;
  void EnableMetrics(bool signal_collection_allowed) override;
  ServiceProxy* GetServiceProxy() override;
  DatabaseClient* GetDatabaseClient() override;
  bool IsPlatformInitialized() override;

 private:
  friend class SegmentationPlatformServiceImplTest;
  friend class TestServicesForPlatform;

  void OnDatabaseInitialized(bool success);

  // Must only be invoked with a valid SegmentInfo.
  void OnSegmentationModelUpdated(proto::SegmentInfo segment_info,
                                  std::optional<int64_t> old_model_version);

  // Callback sent to child classes to notify when model results need to be
  // refreshed. For example, when history is cleared.
  void OnModelRefreshNeeded();

  // Called when service status changes.
  void OnServiceStatusChanged();

  // Task that runs every day or at startup to keep the platform data updated.
  void RunDailyTasks(bool is_startup);

  // Creates SegmentResultProvider for all configs.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
  CreateSegmentResultProviders();

  // Creates SegmentResultProvider.
  std::unique_ptr<SegmentResultProvider> CreateSegmentResultProvider();
  std::unique_ptr<ModelProviderFactory> model_provider_factory_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<base::Clock> clock_;
  const PlatformOptions platform_options_;

  // Temporarily stored till initialization and moved to `execution_service_`.
  std::unique_ptr<processing::InputDelegateHolder> input_delegate_holder_;

  std::unique_ptr<FieldTrialRegister> field_trial_register_;

  std::unique_ptr<StorageService> storage_service_;
  // Storage initialization status.
  std::optional<bool> storage_init_status_;

  // Signal processing.
  SignalHandler signal_handler_;

  ExecutionService execution_service_;

  std::unique_ptr<DatabaseClientImpl> database_client_;

  // Segment selection.
  // TODO(shaktisahu): Determine safe destruction ordering between
  // SegmentSelectorImpl and ModelExecutionSchedulerImpl.
  base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>
      segment_selectors_;

  // Records field trials for all configs.
  std::unique_ptr<FieldTrialRecorder> field_trial_recorder_;

  // For routing requests to the right handler.
  std::unique_ptr<RequestDispatcher> request_dispatcher_;

  // Refreshes model results.
  std::unique_ptr<ResultRefreshManager> result_refresh_manager_;

  // Segment results.
  std::unique_ptr<SegmentScoreProvider> segment_score_provider_;

  std::unique_ptr<ServiceProxyImpl> proxy_;

  // Prefs Migration
  std::unique_ptr<PrefsMigrator> prefs_migrator_;

  // PrefService from profile.
  raw_ptr<PrefService> profile_prefs_;

  // For metrics only:
  const base::Time creation_time_;
  base::Time init_time_;

  // For caching any method calls that were received before initialization.
  base::circular_deque<base::OnceClosure> pending_actions_;

  base::WeakPtrFactory<SegmentationPlatformServiceImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
