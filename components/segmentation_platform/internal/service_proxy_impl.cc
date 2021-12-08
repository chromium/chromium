// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/service_proxy_impl.h"

namespace segmentation_platform {

ServiceProxyImpl::ServiceProxyImpl(SegmentationPlatformService* service)
    : is_service_initialized_(false),
      service_status_flag_(0),
      service_(service) {}

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
}

void ServiceProxyImpl::GetServiceStatus() {
  OnServiceStatusChanged(is_service_initialized_, service_status_flag_);
}

void ServiceProxyImpl::GetSelectedSegment(
    const std::string& segmentation_key,
    SegmentationPlatformService::SegmentSelectionCallback callback) {
  service_->GetSelectedSegment(segmentation_key, std::move(callback));
}

}  // namespace segmentation_platform
