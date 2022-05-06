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
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/service_proxy_impl.h"
#include "components/segmentation_platform/internal/signals/signal_handler.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

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

struct Config;
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

    leveldb_proto::ProtoDatabaseProvider* db_provider = nullptr;
    history::HistoryService* history_service = nullptr;
    base::FilePath storage_dir;
    PrefService* profile_prefs = nullptr;
    PrefService* local_state = nullptr;

    std::unique_ptr<ModelProviderFactory> model_provider;
    UkmDataManager* ukm_data_manager = nullptr;
    std::vector<std::unique_ptr<Config>> configs;
    std::unique_ptr<FieldTrialRegister> field_trial_register;

    scoped_refptr<base::SequencedTaskRunner> task_runner;
    base::Clock* clock = nullptr;

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
  SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) override;
  void EnableMetrics(bool signal_collection_allowed) override;
  ServiceProxy* GetServiceProxy() override;
  bool IsPlatformInitialized() override;

 private:
  friend class SegmentationPlatformServiceImplTest;
  friend class TestServicesForPlatform;

  void OnDatabaseInitialized(bool success);

  // Must only be invoked with a valid SegmentInfo.
  void OnSegmentationModelUpdated(proto::SegmentInfo segment_info);

  // Callback sent to child classes to notify when model results need to be
  // refreshed. For example, when history is cleared.
  void OnModelRefreshNeeded();

  // Called when service status changes.
  void OnServiceStatusChanged();

  // Task that runs every day or at startup to keep the platform data updated.
  void RunDailyTasks(bool is_startup);

  std::unique_ptr<ModelProviderFactory> model_provider_factory_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<base::Clock> clock_;
  const PlatformOptions platform_options_;

  // Config.
  std::vector<std::unique_ptr<Config>> configs_;
  base::flat_set<optimization_guide::proto::OptimizationTarget>
      all_segment_ids_;
  std::unique_ptr<FieldTrialRegister> field_trial_register_;

  std::unique_ptr<StorageService> storage_service_;
  bool storage_initialized_ = false;

  // Signal processing.
  SignalHandler signal_handler_;

  // Segment selection.
  // TODO(shaktisahu): Determine safe destruction ordering between
  // SegmentSelectorImpl and ModelExecutionSchedulerImpl.
  base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>
      segment_selectors_;

  // Segment results.
  std::unique_ptr<SegmentScoreProvider> segment_score_provider_;

  ExecutionService execution_service_;

  std::unique_ptr<ServiceProxyImpl> proxy_;

  // PrefService from profile.
  raw_ptr<PrefService> profile_prefs_;

  // For metrics only:
  const base::Time creation_time_;
  base::Time init_time_;

  base::WeakPtrFactory<SegmentationPlatformServiceImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
