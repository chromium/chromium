// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/service_proxy.h"

namespace segmentation_platform {
using proto::SegmentId;

struct Config;
class ExecutionService;
class SegmentSelectorImpl;
class SegmentResultProvider;
class SignalStorageConfig;

// A helper class to expose internals of the segmentationss service to a logging
// component and/or debug UI.
class ServiceProxyImpl : public ServiceProxy,
                         public ModelExecutionScheduler::Observer {
 public:
  ServiceProxyImpl(
      SegmentInfoDatabase* segment_db,
      SignalStorageConfig* signal_storage_config,
      const std::vector<std::unique_ptr<Config>>* configs,
      const PlatformOptions& options,
      base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>*
          segment_selectors);
  ~ServiceProxyImpl() override;

  void AddObserver(ServiceProxy::Observer* observer) override;
  void RemoveObserver(ServiceProxy::Observer* observer) override;

  ServiceProxyImpl(const ServiceProxyImpl& other) = delete;
  ServiceProxyImpl& operator=(const ServiceProxyImpl& other) = delete;

  void SetExecutionService(ExecutionService* model_execution_scheduler);

  // ServiceProxy impl.
  void GetServiceStatus() override;
  void ExecuteModel(SegmentId segment_id) override;
  void OverwriteResult(SegmentId segment_id, float result) override;
  void SetSelectedSegment(const std::string& segmentation_key,
                          SegmentId segment_id) override;

  // Called when segmentation service status changed.
  void OnServiceStatusChanged(bool is_initialized, int status_flag);

 private:
  // Called to update observers with new segmentation info. If
  // |update_service_status| is true, status about the segmentation service will
  // be sent.
  void UpdateObservers(bool update_service_status);

  void OnSegmentInfoFetchedForExecution(
      std::optional<proto::SegmentInfo> segment_info);

  //  Called after retrieving all the segmentation info from the DB.
  void OnGetAllSegmentationInfo(
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_info_list);

  // ModelExecutionScheduler::Observer overrides.
  void OnModelExecutionCompleted(SegmentId segment_id) override;

  const bool force_refresh_results_ = false;
  bool is_service_initialized_ = false;
  int service_status_flag_ = 0;
  const raw_ptr<SegmentInfoDatabase, DanglingUntriaged> segment_db_;
  const raw_ptr<SignalStorageConfig> signal_storage_config_;
  const raw_ptr<const std::vector<std::unique_ptr<Config>>> configs_;
  base::ObserverList<ServiceProxy::Observer> observers_;
  raw_ptr<ExecutionService> execution_service_{nullptr};
  const raw_ptr<
      base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>>
      segment_selectors_;
  std::unique_ptr<SegmentResultProvider> segment_result_provider_;

  base::WeakPtrFactory<ServiceProxyImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_
