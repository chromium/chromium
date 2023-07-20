// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/lightweight_detector.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan::internal {

class LightweightDetectorTest : public testing::Test {};

TEST_F(LightweightDetectorTest, EncodeMetadataId) {
  static_assert(sizeof(LightweightDetector::MetadataId) == 4,
                "Update the test to sufficiently cover the MetadataId range.");

  LightweightDetector::MetadataId id;

  for (id = 0; id < 4; ++id) {
    EXPECT_EQ(id, LightweightDetector::ExtractMetadataId(
                      LightweightDetector::EncodeMetadataId(id)));
  }

  for (id = 0x100; id < 0x100 + 4; ++id) {
    EXPECT_EQ(id, LightweightDetector::ExtractMetadataId(
                      LightweightDetector::EncodeMetadataId(id)));
  }

  for (id = 0x10000; id < 0x10000 + 4; ++id) {
    EXPECT_EQ(id, LightweightDetector::ExtractMetadataId(
                      LightweightDetector::EncodeMetadataId(id)));
  }

  for (id = 0x1000000; id < 0x1000000 + 4; ++id) {
    EXPECT_EQ(id, LightweightDetector::ExtractMetadataId(
                      LightweightDetector::EncodeMetadataId(id)));
  }

  id = 0xffffffff;
  EXPECT_EQ(id, LightweightDetector::ExtractMetadataId(
                    LightweightDetector::EncodeMetadataId(id)));
}

}  // namespace gwp_asan::internal
