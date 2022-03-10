// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/public/service_proxy.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {
struct Config;
class SignalStorageConfig;
class ModelExecutionScheduler;
class SegmentSelectorImpl;

// A helper class to expose internals of the segmentationss service to a logging
// component and/or debug UI.
class ServiceProxyImpl : public ServiceProxy {
 public:
  ServiceProxyImpl(
      SegmentInfoDatabase* segment_db,
      SignalStorageConfig* signal_storage_config,
      std::vector<std::unique_ptr<Config>>* configs,
      base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>*
          segment_selectors);
  ~ServiceProxyImpl() override;

  void AddObserver(ServiceProxy::Observer* observer) override;
  void RemoveObserver(ServiceProxy::Observer* observer) override;

  ServiceProxyImpl(const ServiceProxyImpl& other) = delete;
  ServiceProxyImpl& operator=(const ServiceProxyImpl& other) = delete;

  void SetModelExecutionScheduler(
      ModelExecutionScheduler* model_execution_scheduler);

  // ServiceProxy impl.
  void GetServiceStatus() override;
  void ExecuteModel(OptimizationTarget segment_id) override;
  void OverwriteResult(OptimizationTarget segment_id, float result) override;
  void SetSelectedSegment(const std::string& segmentation_key,
                          OptimizationTarget segment_id) override;

  // Called when segmentation service status changed.
  void OnServiceStatusChanged(bool is_initialized, int status_flag);

 private:
  // Called to update observers with new segmentation info. If
  // |update_service_status| is true, status about the segmentation service will
  // be sent.
  void UpdateObservers(bool update_service_status);

  void OnSegmentInfoFetchedForExecution(
      absl::optional<proto::SegmentInfo> segment_info);

  //  Called after retrieving all the segmentation info from the DB.
  void OnGetAllSegmentationInfo(
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_info);

  bool is_service_initialized_ = false;
  int service_status_flag_ = 0;
  raw_ptr<SegmentInfoDatabase> segment_db_;
  raw_ptr<SignalStorageConfig> signal_storage_config_;
  raw_ptr<std::vector<std::unique_ptr<Config>>> configs_;
  base::ObserverList<ServiceProxy::Observer> observers_;
  raw_ptr<ModelExecutionScheduler> model_execution_scheduler_{nullptr};
  raw_ptr<base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>>
      segment_selectors_;

  base::WeakPtrFactory<ServiceProxyImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_
