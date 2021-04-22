// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class SegmentationPlatformServiceImplTest : public testing::Test {
 public:
  SegmentationPlatformServiceImplTest() = default;
  ~SegmentationPlatformServiceImplTest() override = default;

  void SetUp() override {
    segmentation_platform_service_impl_ =
        std::make_unique<SegmentationPlatformServiceImpl>();
  }

 private:
  std::unique_ptr<SegmentationPlatformServiceImpl>
      segmentation_platform_service_impl_;
};

TEST_F(SegmentationPlatformServiceImplTest, DummyTest) {}

}  // namespace segmentation_platform
