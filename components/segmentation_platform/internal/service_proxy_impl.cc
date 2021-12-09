// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/service_proxy_impl.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

namespace segmentation_platform {

// static
std::string ServiceProxyImpl::SegmentInfoToString(
    const proto::SegmentInfo& segment_info) {
  std::string result;
  if (segment_info.has_segment_id()) {
    result = "segment_id: " +
             optimization_guide::GetStringNameForOptimizationTarget(
                 segment_info.segment_id()) +
             "\n";
  }
  if (segment_info.has_prediction_result()) {
    const auto prediction_result = segment_info.prediction_result();
    std::string prediction_result_str = base::StringPrintf(
        "prediction_result: { result: %f, timestamp_us: %" PRId64 " }\n",
        prediction_result.has_result() ? prediction_result.result() : 0,
        prediction_result.has_timestamp_us() ? prediction_result.timestamp_us()
                                             : 0);
    result.append(prediction_result_str);
  }
  return result;
}

ServiceProxyImpl::ServiceProxyImpl(SegmentationPlatformService* service,
                                   SegmentInfoDatabase* segment_db)
    : is_service_initialized_(false),
      service_status_flag_(0),
      service_(service),
      segment_db_(segment_db) {}

ServiceProxyImpl::~ServiceProxyImpl() = default;

void ServiceProxyImpl::AddObserver(ServiceProxy::Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceProxyImpl::RemoveObserver(ServiceProxy::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceProxyImpl::OnServiceStatusChanged(bool is_initialized,
                                              int status_flag) {
  is_service_initialized_ = is_initialized;
  service_status_flag_ = status_flag;
  for (Observer& obs : observers_)
    obs.OnServiceStatusChanged(is_initialized, status_flag);

  if (segment_db_ &&
      (static_cast<int>(ServiceStatus::kSegmentationInfoDbInitialized) &
       status_flag)) {
    segment_db_->GetAllSegmentInfo(
        base::BindOnce(&ServiceProxyImpl::OnGetAllSegmentationInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ServiceProxyImpl::GetServiceStatus() {
  OnServiceStatusChanged(is_service_initialized_, service_status_flag_);
}

void ServiceProxyImpl::GetSelectedSegment(
    const std::string& segmentation_key,
    SegmentationPlatformService::SegmentSelectionCallback callback) {
  service_->GetSelectedSegment(segmentation_key, std::move(callback));
}

//  Called after retrieving all the segmentation info from the DB.
void ServiceProxyImpl::OnGetAllSegmentationInfo(
    std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
        segment_infos) {
  std::vector<std::string> result;
  for (const auto& segment_info : segment_infos) {
    result.emplace_back(SegmentInfoToString(segment_info.second));
  }

  for (Observer& obs : observers_)
    obs.OnSegmentInfoAvailable(result);
}

}  // namespace segmentation_platform
