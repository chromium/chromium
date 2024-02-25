// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/allocator_state.h"

#include <limits>

#include "base/memory/page_size.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan {
namespace internal {

using GetMetadataReturnType = AllocatorState::GetMetadataReturnType;

static constexpr size_t kMaxMetadata = AllocatorState::kMaxMetadata;
static constexpr size_t kMaxRequestedSlots = AllocatorState::kMaxRequestedSlots;
static constexpr size_t kMaxReservedSlots = AllocatorState::kMaxReservedSlots;

class AllocatorStateTest : public testing::Test {
 protected:
  void InitializeState(size_t page_size,
                       size_t num_metadata,
                       size_t total_requested_pages,
                       size_t total_reserved_pages,
                       int base_addr_offset = 0,
                       int first_page_offset = 0,
                       int end_addr_offset = 0) {
    state_.page_size = page_size;
    state_.num_metadata = num_metadata;
    state_.total_requested_pages = total_requested_pages;
    state_.total_reserved_pages = total_reserved_pages;

    // Some arbitrary page-aligned address
    const uintptr_t base = page_size * 10;
    state_.pages_base_addr = base_addr_offset + base;
    state_.first_page_addr = first_page_offset + base + page_size;
    state_.pages_end_addr =
        end_addr_offset + base + page_size * (total_reserved_pages * 2 + 1);

    // An invalid address, but it's never dereferenced in AllocatorState.
    state_.metadata_addr = 0x1234;
    state_.slot_to_metadata_addr = 0x1234;
  }

  AllocatorState state_;
};

TEST_F(AllocatorStateTest, Valid) {
  InitializeState(base::GetPageSize(), 1, 1, 1);
  EXPECT_TRUE(state_.IsValid());
  InitializeState(base::GetPageSize(), 1, kMaxRequestedSlots,
                  kMaxRequestedSlots);
  EXPECT_TRUE(state_.IsValid());
  InitializeState(base::GetPageSize(), 1, kMaxRequestedSlots,
                  kMaxReservedSlots);
  EXPECT_TRUE(state_.IsValid());
  InitializeState(base::GetPageSize(), kMaxMetadata, kMaxRequestedSlots,
                  kMaxRequestedSlots);
  EXPECT_TRUE(state_.IsValid());
  InitializeState(base::GetPageSize(), kMaxMetadata, kMaxRequestedSlots,
                  kMaxReservedSlots);
  EXPECT_TRUE(state_.IsValid());
}

TEST_F(AllocatorStateTest, InvalidNumMetadata) {
  InitializeState(base::GetPageSize(), 0, kMaxRequestedSlots,
                  kMaxReservedSlots);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), kMaxMetadata + 1, kMaxRequestedSlots,
                  kMaxReservedSlots);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 2, 1, 1);
  EXPECT_FALSE(state_.IsValid());
}

TEST_F(AllocatorStateTest, InvalidNumPages) {
  InitializeState(base::GetPageSize(), 1, 0, 0);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 1, 0, 1);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 1, 1, 0);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 1, kMaxRequestedSlots + 1,
                  kMaxReservedSlots);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 1, kMaxRequestedSlots,
                  kMaxReservedSlots + 1);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 1, kMaxRequestedSlots + 1,
                  kMaxReservedSlots + 1);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 1, std::numeric_limits<size_t>::max(),
                  std::numeric_limits<size_t>::max());
  EXPECT_FALSE(state_.IsValid());
}

TEST_F(AllocatorStateTest, InvalidPageSize) {
  InitializeState(0, 1, 1, 1);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize() + 1, 1, 1, 1);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize() * 2, 1, 1, 1);
  EXPECT_FALSE(state_.IsValid());
}

TEST_F(AllocatorStateTest, InvalidAddresses) {
  // Invalid [pages_base_addr, first_page_addr, pages_end_addr]
  InitializeState(base::GetPageSize(), 1, 1, 1, 1, 1, 1);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 1, 1, 1, 1, 0, 0);
  EXPECT_FALSE(state_.IsValid());
  InitializeState(base::GetPageSize(), 1, 1, 1, 0, 1, 0);
  EXPECT_FALSE(state_.IsValid());
  InitializeState(base::GetPageSize(), 1, 1, 1, 0, 0, 1);
  EXPECT_FALSE(state_.IsValid());

  InitializeState(base::GetPageSize(), 1, 1, 1, base::GetPageSize(), 0, 0);
  EXPECT_FALSE(state_.IsValid());
  InitializeState(base::GetPageSize(), 1, 1, 1, 0, base::GetPageSize(), 0);
  EXPECT_FALSE(state_.IsValid());
  InitializeState(base::GetPageSize(), 1, 1, 1, 0, 0, base::GetPageSize());
  EXPECT_FALSE(state_.IsValid());
}

TEST_F(AllocatorStateTest, GetNearestValidPageEdgeCases) {
  InitializeState(base::GetPageSize(), kMaxMetadata, kMaxRequestedSlots,
                  kMaxReservedSlots);
  EXPECT_TRUE(state_.IsValid());

  EXPECT_EQ(
      state_.GetPageAddr(state_.GetNearestValidPage(state_.pages_base_addr)),
      state_.first_page_addr);
  EXPECT_EQ(
      state_.GetPageAddr(state_.GetNearestValidPage(state_.pages_end_addr - 1)),
      state_.pages_end_addr - (2 * state_.page_size));
}

TEST_F(AllocatorStateTest, GetErrorTypeEdgeCases) {
  InitializeState(base::GetPageSize(), kMaxMetadata, kMaxRequestedSlots,
                  kMaxReservedSlots);
  EXPECT_TRUE(state_.IsValid());

  EXPECT_EQ(state_.GetErrorType(state_.pages_base_addr, true, false),
            AllocatorState::ErrorType::kBufferUnderflow);
  EXPECT_EQ(state_.GetErrorType(state_.pages_end_addr - 1, true, false),
            AllocatorState::ErrorType::kBufferOverflow);
}

// Correctly handle the edge case when a free() occurs on a page that has never
// been allocated.
TEST_F(AllocatorStateTest, GetErrorTypeFreeInvalidAddressEdgeCase) {
  InitializeState(base::GetPageSize(), kMaxMetadata, kMaxRequestedSlots,
                  kMaxReservedSlots);
  EXPECT_TRUE(state_.IsValid());

  state_.free_invalid_address = state_.first_page_addr;
  EXPECT_EQ(state_.GetErrorType(state_.first_page_addr, false, false),
            AllocatorState::ErrorType::kFreeInvalidAddress);
}

TEST_F(AllocatorStateTest, GetMetadataForAddress) {
  constexpr size_t kTestSlots = 10;
  InitializeState(base::GetPageSize(), 2, kTestSlots, kTestSlots);

  AllocatorState::SlotMetadata md[2];
  md[0].alloc_ptr = state_.first_page_addr;

  AllocatorState::MetadataIdx slot_to_metadata[kTestSlots];

  AllocatorState::MetadataIdx idx;
  std::string error;
#if defined(GTEST_HAS_DEATH_TEST)
  EXPECT_DEATH(
      {
        state_.GetMetadataForAddress(state_.pages_base_addr - 1, md,
                                     slot_to_metadata, &idx, &error);
      },
      "");
#endif

  slot_to_metadata[0] = AllocatorState::kInvalidMetadataIdx;
  EXPECT_EQ(state_.GetMetadataForAddress(state_.first_page_addr, md,
                                         slot_to_metadata, &idx, &error),
            GetMetadataReturnType::kGwpAsanCrashWithMissingMetadata);

  slot_to_metadata[0] = 0;
  ASSERT_EQ(state_.GetMetadataForAddress(state_.first_page_addr, md,
                                         slot_to_metadata, &idx, &error),
            GetMetadataReturnType::kGwpAsanCrash);
  EXPECT_EQ(idx, 0);

  slot_to_metadata[0] = kTestSlots;
  EXPECT_EQ(state_.GetMetadataForAddress(state_.first_page_addr, md,
                                         slot_to_metadata, &idx, &error),
            GetMetadataReturnType::kErrorBadMetadataIndex);

  // Metadata[0] is for slot 1, slot_to_metadata_idx[0] point to metadata 0.
  md[0].alloc_ptr = state_.first_page_addr + state_.page_size * 2;
  slot_to_metadata[0] = 0;
  EXPECT_EQ(state_.GetMetadataForAddress(state_.first_page_addr, md,
                                         slot_to_metadata, &idx, &error),
            GetMetadataReturnType::kErrorOutdatedMetadataIndex);

  // It's impossible to trigger kErrorBadSlotIndex.
}

}  // namespace internal
}  // namespace gwp_asan
