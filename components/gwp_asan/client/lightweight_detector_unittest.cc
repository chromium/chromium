// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan::internal {

constexpr size_t kMaxLightweightDetectorMetadata = 1;
class LightweightDetectorTest : public testing::Test {
 public:
  LightweightDetectorTest()
      : detector_(LightweightDetectorMode::kBrpQuarantine,
                  kMaxLightweightDetectorMetadata) {}

 protected:
  LightweightDetector detector_;
};

TEST_F(LightweightDetectorTest, PoisonAlloc) {
  uint64_t alloc;

  detector_.RecordLightweightDeallocation(&alloc, sizeof(alloc));
  auto metadata_id = LightweightDetectorState::ExtractMetadataId(alloc);
  EXPECT_TRUE(metadata_id.has_value());

  auto& metadata = detector_.state_.GetSlotMetadataById(
      *metadata_id, detector_.metadata_.get());
  EXPECT_EQ(metadata.alloc_ptr, reinterpret_cast<uintptr_t>(&alloc));
  EXPECT_EQ(metadata.alloc_size, sizeof(alloc));
  EXPECT_EQ(metadata.dealloc.trace_collected, true);
  EXPECT_NE(metadata.dealloc.trace_len, 0u);
}

TEST_F(LightweightDetectorTest, PoisonAllocUnaligned) {
  // Allocations that aren't 64-bit aligned.
  uint8_t alloc1[7];
  uint8_t alloc2[9];

  detector_.RecordLightweightDeallocation(&alloc1, sizeof(alloc1));
  detector_.RecordLightweightDeallocation(&alloc2, sizeof(alloc2));

  for (auto byte : alloc1) {
    EXPECT_EQ(byte, LightweightDetectorState::kMetadataRemainder);
  }
  EXPECT_EQ(alloc2[sizeof(alloc2) - 1],
            LightweightDetectorState::kMetadataRemainder);
}

TEST_F(LightweightDetectorTest, SlotReuse) {
  uint64_t alloc1;
  uint64_t alloc2;

  detector_.RecordLightweightDeallocation(&alloc1, sizeof(alloc1));
  auto alloc1_metadata_id = LightweightDetectorState::ExtractMetadataId(alloc1);
  EXPECT_TRUE(alloc1_metadata_id.has_value());
  auto& metadata_alloc1 = detector_.state_.GetSlotMetadataById(
      *alloc1_metadata_id, detector_.metadata_.get());

  detector_.RecordLightweightDeallocation(&alloc2, sizeof(alloc2));
  auto alloc2_metadata_id = LightweightDetectorState::ExtractMetadataId(alloc2);
  auto& metadata_alloc2 = detector_.state_.GetSlotMetadataById(
      *alloc2_metadata_id, detector_.metadata_.get());

  // Since there's only one slot, it should be reused.
  EXPECT_EQ(&metadata_alloc1, &metadata_alloc2);
  EXPECT_NE(metadata_alloc1.id, alloc1_metadata_id);
  EXPECT_EQ(metadata_alloc2.id, alloc2_metadata_id);
}

}  // namespace gwp_asan::internal
