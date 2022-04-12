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
class ModelProviderFactory;
class SegmentSelectorImpl;
class SegmentScoreProvider;
class UkmDataManager;

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
      std::unique_ptr<StorageService> storage_service,
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
  friend class TestServicesForPlatform;

  void OnDatabaseInitialized(bool success);

  // Must only be invoked with a valid SegmentInfo.
  void OnSegmentationModelUpdated(proto::SegmentInfo segment_info);

  // Called when service status changes.
  void OnServiceStatusChanged();

  std::unique_ptr<ModelProviderFactory> model_provider_factory_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<base::Clock> clock_;
  const PlatformOptions platform_options_;

  // Config.
  std::vector<std::unique_ptr<Config>> configs_;
  base::flat_set<optimization_guide::proto::OptimizationTarget>
      all_segment_ids_;

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

  base::WeakPtrFactory<SegmentationPlatformServiceImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
