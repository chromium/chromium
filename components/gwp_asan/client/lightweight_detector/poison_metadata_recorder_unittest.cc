// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/client/lightweight_detector/poison_metadata_recorder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan::internal::lud {

namespace {
constexpr size_t kMaxLightweightDetectorMetadata = 1;
}

class PoisonMetadataRecorderTest : public testing::Test {
 protected:
  PoisonMetadataRecorder recorder_{LightweightDetectorMode::kBrpQuarantine,
                                   kMaxLightweightDetectorMetadata};
};

TEST_F(PoisonMetadataRecorderTest, PoisonAlloc) {
  uint64_t alloc;

  recorder_.RecordAndZap(&alloc, sizeof(alloc));
  auto metadata_id = LightweightDetectorState::ExtractMetadataId(alloc);
  EXPECT_TRUE(metadata_id.has_value());

  auto& metadata = recorder_.state_.GetSlotMetadataById(
      *metadata_id, recorder_.metadata_.get());
  EXPECT_EQ(metadata.alloc_ptr, reinterpret_cast<uintptr_t>(&alloc));
  EXPECT_EQ(metadata.alloc_size, sizeof(alloc));
  EXPECT_EQ(metadata.dealloc.trace_collected, true);
  EXPECT_NE(metadata.dealloc.trace_len, 0u);
}

TEST_F(PoisonMetadataRecorderTest, PoisonAllocUnaligned) {
  // Allocations that aren't 64-bit aligned in size. The addresses themselves
  // are still expected to be aligned as if they were heap allocations.
  alignas(4) uint8_t alloc1[7];
  alignas(8) uint8_t alloc2[9];

  recorder_.RecordAndZap(&alloc1, sizeof(alloc1));
  recorder_.RecordAndZap(&alloc2, sizeof(alloc2));

  for (auto byte : alloc1) {
    EXPECT_EQ(byte, LightweightDetectorState::kMetadataRemainder);
  }
  EXPECT_EQ(alloc2[sizeof(alloc2) - 1],
            LightweightDetectorState::kMetadataRemainder);
}

TEST_F(PoisonMetadataRecorderTest, SlotReuse) {
  uint64_t alloc1;
  uint64_t alloc2;

  recorder_.RecordAndZap(&alloc1, sizeof(alloc1));
  auto alloc1_metadata_id = LightweightDetectorState::ExtractMetadataId(alloc1);
  EXPECT_TRUE(alloc1_metadata_id.has_value());
  auto& metadata_alloc1 = recorder_.state_.GetSlotMetadataById(
      *alloc1_metadata_id, recorder_.metadata_.get());

  recorder_.RecordAndZap(&alloc2, sizeof(alloc2));
  auto alloc2_metadata_id = LightweightDetectorState::ExtractMetadataId(alloc2);
  auto& metadata_alloc2 = recorder_.state_.GetSlotMetadataById(
      *alloc2_metadata_id, recorder_.metadata_.get());

  // Since there's only one slot, it should be reused.
  EXPECT_EQ(&metadata_alloc1, &metadata_alloc2);
  EXPECT_NE(metadata_alloc1.id, alloc1_metadata_id);
  EXPECT_EQ(metadata_alloc2.id, alloc2_metadata_id);
}

}  // namespace gwp_asan::internal::lud
