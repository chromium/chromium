// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/service_proxy_impl.h"

#include <inttypes.h>
#include <sstream>

#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"
#include "components/segmentation_platform/public/config.h"

#include "base/logging.h"
namespace segmentation_platform {

namespace {
std::string SegmentMetadataToString(const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_model_metadata())
    return std::string();

  return "model_metadata: { " +
         metadata_utils::SegmetationModelMetadataToString(
             segment_info.model_metadata()) +
         " }";
}

std::string PredictionResultToString(const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_prediction_result())
    return std::string();
  const auto prediction_result = segment_info.prediction_result();
  base::Time time;
  if (prediction_result.has_timestamp_us()) {
    time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(prediction_result.timestamp_us()));
  }
  std::ostringstream time_string;
  time_string << time;
  return base::StringPrintf(
      "result: %f, time: %s",
      prediction_result.has_result() ? prediction_result.result() : 0,
      time_string.str().c_str());
}
}  // namespace

ServiceProxyImpl::ServiceProxyImpl(
    SegmentInfoDatabase* segment_db,
    SignalStorageConfig* signal_storage_config,
    std::vector<std::unique_ptr<Config>>* configs,
    base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>*
        segment_selectors)
    : segment_db_(segment_db),
      signal_storage_config_(signal_storage_config),
      configs_(configs),
      segment_selectors_(segment_selectors) {}

ServiceProxyImpl::~ServiceProxyImpl() = default;

void ServiceProxyImpl::AddObserver(ServiceProxy::Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceProxyImpl::RemoveObserver(ServiceProxy::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceProxyImpl::OnServiceStatusChanged(bool is_initialized,
                                              int status_flag) {
  bool changed = (is_service_initialized_ != is_initialized) ||
                 (service_status_flag_ != status_flag);
  is_service_initialized_ = is_initialized;
  service_status_flag_ = status_flag;
  UpdateObservers(changed);
}

void ServiceProxyImpl::UpdateObservers(bool update_service_status) {
  if (update_service_status) {
    for (Observer& obs : observers_)
      obs.OnServiceStatusChanged(is_service_initialized_, service_status_flag_);
  }

  if (segment_db_ &&
      (static_cast<int>(ServiceStatus::kSegmentationInfoDbInitialized) &
       service_status_flag_)) {
    segment_db_->GetAllSegmentInfo(
        base::BindOnce(&ServiceProxyImpl::OnGetAllSegmentationInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ServiceProxyImpl::SetModelExecutionScheduler(
    ModelExecutionScheduler* model_execution_scheduler) {
  model_execution_scheduler_ = model_execution_scheduler;
}

void ServiceProxyImpl::GetServiceStatus() {
  UpdateObservers(true /* update_service_status */);
}

void ServiceProxyImpl::ExecuteModel(OptimizationTarget segment_id) {
  if (!model_execution_scheduler_ ||
      segment_id == OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN) {
    return;
  }
  segment_db_->GetSegmentInfo(
      segment_id,
      base::BindOnce(&ServiceProxyImpl::OnSegmentInfoFetchedForExecution,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ServiceProxyImpl::OnSegmentInfoFetchedForExecution(
    absl::optional<proto::SegmentInfo> segment_info) {
  if (segment_info)
    model_execution_scheduler_->RequestModelExecution(*segment_info);
}

void ServiceProxyImpl::OverwriteResult(OptimizationTarget segment_id,
                                       float result) {
  if (!model_execution_scheduler_)
    return;

  if (result < 0 || result > 1)
    return;

  if (segment_id != OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN) {
    model_execution_scheduler_->OnModelExecutionCompleted(
        segment_id, std::make_pair(result, ModelExecutionStatus::kSuccess));
  }
}

void ServiceProxyImpl::SetSelectedSegment(const std::string& segmentation_key,
                                          OptimizationTarget segment_id) {
  if (!segment_selectors_ ||
      segment_selectors_->find(segmentation_key) == segment_selectors_->end()) {
    return;
  }
  if (segment_id != OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN) {
    auto& selector = segment_selectors_->at(segmentation_key);
    selector->UpdateSelectedSegment(segment_id);
  }
}

void ServiceProxyImpl::OnGetAllSegmentationInfo(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_info) {
  if (!configs_)
    return;

  // Convert the |segment_info| vector to a map for quick lookup.
  base::flat_map<OptimizationTarget, proto::SegmentInfo> optimization_targets;
  for (const auto& info : *segment_info) {
    optimization_targets[info.first] = info.second;
  }

  std::vector<ServiceProxy::ClientInfo> result;
  for (const auto& config : *configs_) {
    OptimizationTarget selected =
        OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;
    if (segment_selectors_ &&
        segment_selectors_->find(config->segmentation_key) !=
            segment_selectors_->end()) {
      absl::optional<optimization_guide::proto::OptimizationTarget> target =
          segment_selectors_->at(config->segmentation_key)
              ->GetCachedSegmentResult()
              .segment;
      if (target.has_value()) {
        selected = target.value();
      }
    }
    result.emplace_back(config->segmentation_key, selected);
    for (const auto& segment_id : config->segment_ids) {
      if (!optimization_targets.contains(segment_id))
        continue;
      const auto& info = optimization_targets[segment_id];
      result.back().segment_status.emplace_back(
          segment_id, SegmentMetadataToString(info),
          PredictionResultToString(info),
          signal_storage_config_
              ? signal_storage_config_->MeetsSignalCollectionRequirement(
                    info.model_metadata())
              : false);
    }
  }

  for (Observer& obs : observers_)
    obs.OnClientInfoAvailable(result);
}

}  // namespace segmentation_platform
