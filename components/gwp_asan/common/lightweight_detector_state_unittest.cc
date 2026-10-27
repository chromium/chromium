// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/lightweight_detector_state.h"

#include "base/test/gtest_util.h"
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
  InitializeState(0, 0x1234);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(kMaxMetadata + 1, 0x1234);
  EXPECT_FALSE(state_.IsValid());
}

TEST_F(LightweightDetectorStateTest, InvalidAddress) {
  InitializeState(kMaxMetadata, 0);
  EXPECT_FALSE(state_.IsValid());
}

TEST_F(LightweightDetectorStateTest, GetSlotMetadataById) {
  LightweightDetectorState::SlotMetadata md[2];
  md[0].id = 0;
  md[1].id = 3;

  InitializeState(2, reinterpret_cast<uintptr_t>(md));
  ASSERT_TRUE(state_.IsValid());

  EXPECT_TRUE(state_.HasMetadataForId(0, md));
  EXPECT_EQ(&state_.GetSlotMetadataById(0, md), &md[0]);
  EXPECT_FALSE(state_.HasMetadataForId(2, md));

  EXPECT_FALSE(state_.HasMetadataForId(1, md));
  EXPECT_TRUE(state_.HasMetadataForId(3, md));
  EXPECT_EQ(&state_.GetSlotMetadataById(3, md), &md[1]);

  InitializeState(0, reinterpret_cast<uintptr_t>(md));
  EXPECT_CHECK_DEATH(state_.GetSlotMetadataById(0, md));
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
