// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_

#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {

// The internal implementation of the SegmentationPlatformService.
class SegmentationPlatformServiceImpl : public SegmentationPlatformService {
 public:
  SegmentationPlatformServiceImpl();
  ~SegmentationPlatformServiceImpl() override;

  // Disallow copy/assign.
  SegmentationPlatformServiceImpl(const SegmentationPlatformServiceImpl&) =
      delete;
  SegmentationPlatformServiceImpl& operator=(
      const SegmentationPlatformServiceImpl&) = delete;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_IMPL_H_
