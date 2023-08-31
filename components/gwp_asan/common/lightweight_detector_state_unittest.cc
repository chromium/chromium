// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/lightweight_detector_state.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan::internal {

static constexpr size_t kMaxMetadata = LightweightDetectorState::kMaxMetadata;

class LightweightDetectorStateTest : public testing::Test {
 protected:
  void InitializeState(size_t num_metadata, uintptr_t metadata_addr) {
    state_.num_metadata = num_metadata;
    state_.metadata_addr = metadata_addr;
  }

  LightweightDetectorState state_;
};

TEST_F(LightweightDetectorStateTest, Valid) {
  InitializeState(1, 0x1234);
  EXPECT_TRUE(state_.IsValid());

  InitializeState(kMaxMetadata, 0x1234);
  EXPECT_TRUE(state_.IsValid());
}

TEST_F(LightweightDetectorStateTest, InvalidNumMetadata) {
  InitializeState(kMaxMetadata + 1, 0x1234);
  EXPECT_FALSE(state_.IsValid());
}

TEST_F(LightweightDetectorStateTest, InvalidAddress) {
  InitializeState(kMaxMetadata, 0);
  EXPECT_FALSE(state_.IsValid());
}

TEST_F(LightweightDetectorStateTest, EncodeMetadataId) {
  static_assert(sizeof(LightweightDetectorState::MetadataId) == 4,
                "Update the test to sufficiently cover the MetadataId range.");

  LightweightDetectorState::MetadataId id;

  for (id = 0; id < 4; ++id) {
    EXPECT_EQ(id, LightweightDetectorState::ExtractMetadataId(
                      LightweightDetectorState::EncodeMetadataId(id)));
  }

  for (id = 0x100; id < 0x100 + 4; ++id) {
    EXPECT_EQ(id, LightweightDetectorState::ExtractMetadataId(
                      LightweightDetectorState::EncodeMetadataId(id)));
  }

  for (id = 0x10000; id < 0x10000 + 4; ++id) {
    EXPECT_EQ(id, LightweightDetectorState::ExtractMetadataId(
                      LightweightDetectorState::EncodeMetadataId(id)));
  }

  for (id = 0x1000000; id < 0x1000000 + 4; ++id) {
    EXPECT_EQ(id, LightweightDetectorState::ExtractMetadataId(
                      LightweightDetectorState::EncodeMetadataId(id)));
  }

  id = 0xffffffff;
  EXPECT_EQ(id, LightweightDetectorState::ExtractMetadataId(
                    LightweightDetectorState::EncodeMetadataId(id)));
}

}  // namespace gwp_asan::internal
