// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace segmentation_platform {

// The core class of segmentation platform that integrates all the required
// pieces on the client side.
class SegmentationPlatformService : public KeyedService {
 public:
  SegmentationPlatformService() = default;
  ~SegmentationPlatformService() override = default;

  SegmentationPlatformService(const SegmentationPlatformService&) = delete;
  SegmentationPlatformService& operator=(const SegmentationPlatformService&) =
      delete;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_
