// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SERVICE_PROXY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SERVICE_PROXY_H_

#include <utility>
#include <vector>

#include "base/observer_list_types.h"

namespace segmentation_platform {

// A helper class to expose internals of the segmentationss service to a logging
// component and/or debug UI.
class ServiceProxy {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the servoice status changes.
    virtual void OnServiceStatusChanged(bool is_initialized, int status_flag) {}
    virtual void OnSegmentInfoAvailable(
        const std::vector<std::pair<std::string, std::string>>& segment_info) {}
  };

  virtual ~ServiceProxy() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  ServiceProxy(const ServiceProxy& other) = delete;
  ServiceProxy& operator=(const ServiceProxy& other) = delete;

  // Returns the current status of the segmentation service.
  virtual void GetServiceStatus() = 0;

 protected:
  ServiceProxy() = default;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SERVICE_PROXY_H_