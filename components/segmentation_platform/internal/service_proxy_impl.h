// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_

#include "base/observer_list.h"
#include "components/segmentation_platform/public/service_proxy.h"

namespace segmentation_platform {

// A helper class to expose internals of the segmentationss service to a logging
// component and/or debug UI.
class ServiceProxyImpl : public ServiceProxy {
 public:
  explicit ServiceProxyImpl(SegmentationPlatformService* service);
  ~ServiceProxyImpl() override;

  void AddObserver(ServiceProxy::Observer* observer) override;
  void RemoveObserver(ServiceProxy::Observer* observer) override;

  ServiceProxyImpl(const ServiceProxyImpl& other) = delete;
  ServiceProxyImpl& operator=(const ServiceProxyImpl& other) = delete;

  // Returns the current status of the segmentation service.
  void GetServiceStatus() override;
  void GetSelectedSegment(
      const std::string& segmentation_key,
      SegmentationPlatformService::SegmentSelectionCallback callback) override;

  // Called when segmentation service status changed.
  void OnServiceStatusChanged(bool is_initialized, int status_flag);

 private:
  bool is_service_initialized_;
  int service_status_flag_;
  SegmentationPlatformService* service_;
  base::ObserverList<ServiceProxy::Observer> observers_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SERVICE_PROXY_IMPL_H_
