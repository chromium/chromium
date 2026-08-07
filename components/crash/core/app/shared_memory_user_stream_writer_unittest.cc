// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/shared_memory_user_stream_writer.h"

#include <algorithm>
#include <limits>
#include <optional>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/test/gtest_util.h"
#include "components/crash/core/common/shared_memory_user_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter {
namespace {

using testing::Each;

// Helper to access the slot header within a shared memory mapping.
const internal::SharedMemoryUserStreamSlotHeader* GetSlotHeader(
    const base::ReadOnlySharedMemoryMapping& mapping,
    uint32_t slot_header_offset) {
  return reinterpret_cast<const internal::SharedMemoryUserStreamSlotHeader*>(
      mapping.GetMemoryAsSpan<uint8_t>().subspan(slot_header_offset).data());
}

// Helper to access the slot payload span within a shared memory mapping.
base::span<const uint8_t> GetSlotPayloadSpan(
    const base::ReadOnlySharedMemoryMapping& mapping,
    uint32_t slot_header_offset,
    size_t payload_size_bytes) {
  return mapping.GetMemoryAsSpan<uint8_t>().subspan(
      slot_header_offset + sizeof(internal::SharedMemoryUserStreamSlotHeader),
      payload_size_bytes);
}

// Helper to compute the offset of the currently inactive slot.
uint32_t GetInactiveSlotHeaderOffset(
    const internal::SharedMemoryUserStreamHeader* header,
    uint32_t slot_capacity_bytes) {
  const uint32_t slot0_offset = sizeof(internal::SharedMemoryUserStreamHeader);
  const uint32_t slot_size =
      sizeof(internal::SharedMemoryUserStreamSlotHeader) + slot_capacity_bytes;
  const uint32_t slot1_offset = slot0_offset + slot_size;
  return (header->active_slot_header_offset == slot0_offset) ? slot1_offset
                                                             : slot0_offset;
}

TEST(SharedMemoryUserStreamWriterTest, ValidInitializesHeader) {
  constexpr uint32_t kStreamType = 1234;
  constexpr uint32_t kSlotCapacityBytes = 1024;

  SharedMemoryUserStreamWriter writer(kStreamType, kSlotCapacityBytes);

  const base::ReadOnlySharedMemoryRegion region = writer.DuplicateRegion();
  const base::ReadOnlySharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());

  const internal::SharedMemoryUserStreamHeader* const header =
      mapping.GetMemoryAs<internal::SharedMemoryUserStreamHeader>();
  ASSERT_NE(header, nullptr);
  EXPECT_EQ(header->magic, internal::kUserStreamMagic);
  EXPECT_EQ(header->version, internal::kUserStreamVersion);
  EXPECT_EQ(header->user_stream_type, kStreamType);
  EXPECT_EQ(header->active_slot_header_offset,
            internal::kNoActiveSlotHeaderOffset);

  const base::span<const uint8_t> full_memory =
      mapping.GetMemoryAsSpan<const uint8_t>();
  ASSERT_GE(full_memory.size(), sizeof(internal::SharedMemoryUserStreamHeader));
  EXPECT_TRUE(std::ranges::all_of(
      full_memory.subspan(sizeof(internal::SharedMemoryUserStreamHeader)),
      [](uint8_t b) { return b == 0; }));
}

TEST(SharedMemoryUserStreamWriterTest, RejectsInvalidSlotSize) {
  // Below minimum slot size (0).
  EXPECT_CHECK_DEATH(SharedMemoryUserStreamWriter(1, 0));

  // Numeric overflow size.
  EXPECT_CHECK_DEATH(
      SharedMemoryUserStreamWriter(1, std::numeric_limits<uint32_t>::max()));
}

TEST(SharedMemoryUserStreamWriterTest, ConsecutiveWritesPingPongSlots) {
  constexpr uint32_t kSlotCapacityBytes = 64;
  SharedMemoryUserStreamWriter writer(100, kSlotCapacityBytes);

  const base::ReadOnlySharedMemoryRegion read_only_region =
      writer.DuplicateRegion();
  const base::ReadOnlySharedMemoryMapping read_mapping = read_only_region.Map();
  ASSERT_TRUE(read_mapping.IsValid());
  const internal::SharedMemoryUserStreamHeader* const header =
      read_mapping.GetMemoryAs<internal::SharedMemoryUserStreamHeader>();
  ASSERT_NE(header, nullptr);

  // Initially no active slot.
  EXPECT_EQ(header->active_slot_header_offset,
            internal::kNoActiveSlotHeaderOffset);

  const uint32_t slot0_offset = sizeof(internal::SharedMemoryUserStreamHeader);
  const uint32_t slot_size =
      sizeof(internal::SharedMemoryUserStreamSlotHeader) + kSlotCapacityBytes;
  const uint32_t slot1_offset = slot0_offset + slot_size;

  // Write 1: targets first slot.
  ASSERT_TRUE(
      writer.WriteData([](base::span<uint8_t> data) -> std::optional<size_t> {
        std::ranges::fill(data, 0x11);
        return 10;
      }));
  EXPECT_EQ(header->active_slot_header_offset, slot0_offset);
  const internal::SharedMemoryUserStreamSlotHeader* const slot0_header_1 =
      GetSlotHeader(read_mapping, slot0_offset);
  ASSERT_NE(slot0_header_1, nullptr);
  EXPECT_EQ(slot0_header_1->generation, 2u);
  EXPECT_EQ(slot0_header_1->payload_size, 10u);

  // Write 2: targets second slot.
  ASSERT_TRUE(
      writer.WriteData([](base::span<uint8_t> data) -> std::optional<size_t> {
        std::ranges::fill(data, 0x22);
        return 20;
      }));
  EXPECT_EQ(header->active_slot_header_offset, slot1_offset);
  const internal::SharedMemoryUserStreamSlotHeader* const slot1_header_2 =
      GetSlotHeader(read_mapping, slot1_offset);
  ASSERT_NE(slot1_header_2, nullptr);
  EXPECT_EQ(slot1_header_2->generation, 2u);
  EXPECT_EQ(slot1_header_2->payload_size, 20u);

  // Write 3: targets first slot again.
  ASSERT_TRUE(
      writer.WriteData([](base::span<uint8_t> data) -> std::optional<size_t> {
        std::ranges::fill(data, 0x33);
        return 30;
      }));
  EXPECT_EQ(header->active_slot_header_offset, slot0_offset);
  const internal::SharedMemoryUserStreamSlotHeader* const slot0_header_3 =
      GetSlotHeader(read_mapping, slot0_offset);
  ASSERT_NE(slot0_header_3, nullptr);
  EXPECT_EQ(slot0_header_3->generation, 4u);
  EXPECT_EQ(slot0_header_3->payload_size, 30u);
}

TEST(SharedMemoryUserStreamWriterTest, FailedWritePreservesPreviousActiveSlot) {
  constexpr uint32_t kSlotCapacityBytes = 64;
  SharedMemoryUserStreamWriter writer(1, kSlotCapacityBytes);

  const base::ReadOnlySharedMemoryRegion read_only_region =
      writer.DuplicateRegion();
  const base::ReadOnlySharedMemoryMapping read_mapping = read_only_region.Map();
  ASSERT_TRUE(read_mapping.IsValid());
  const internal::SharedMemoryUserStreamHeader* const header =
      read_mapping.GetMemoryAs<internal::SharedMemoryUserStreamHeader>();
  ASSERT_NE(header, nullptr);

  // Successful write to the new active slot.
  ASSERT_TRUE(
      writer.WriteData([](base::span<uint8_t> data) -> std::optional<size_t> {
        std::ranges::fill(data, 0xAA);
        return 16;
      }));
  const uint32_t active_slot_header_offset = header->active_slot_header_offset;
  const internal::SharedMemoryUserStreamSlotHeader* const active_slot_header =
      GetSlotHeader(read_mapping, active_slot_header_offset);
  ASSERT_NE(active_slot_header, nullptr);
  EXPECT_EQ(active_slot_header->generation, 2u);
  EXPECT_EQ(active_slot_header->payload_size, 16u);

  // Producer returns std::nullopt (failure).
  EXPECT_FALSE(
      writer.WriteData([](base::span<uint8_t> data) -> std::optional<size_t> {
        return std::nullopt;
      }));
  // Active slot and its contents must remain untouched.
  EXPECT_EQ(header->active_slot_header_offset, active_slot_header_offset);
  EXPECT_EQ(active_slot_header->generation, 2u);
  EXPECT_EQ(active_slot_header->payload_size, 16u);
  // The inactive slot attempted an update, so its generation should have
  // incremented to 2 (even/clean state).
  const uint32_t inactive_slot_header_offset =
      GetInactiveSlotHeaderOffset(header, kSlotCapacityBytes);
  EXPECT_EQ(
      GetSlotHeader(read_mapping, inactive_slot_header_offset)->generation, 2u);

  // Producer attempts write exceeding slot size (contract violation).
  EXPECT_CHECK_DEATH(
      writer.WriteData([](base::span<uint8_t> data) -> std::optional<size_t> {
        return kSlotCapacityBytes + 1;
      }));
  EXPECT_EQ(header->active_slot_header_offset, active_slot_header_offset);
  EXPECT_EQ(active_slot_header->generation, 2u);
  EXPECT_EQ(active_slot_header->payload_size, 16u);

  // Verify active slot data was preserved intact.
  const base::span<const uint8_t> active_span =
      GetSlotPayloadSpan(read_mapping, active_slot_header_offset, 16u);
  EXPECT_THAT(active_span, Each(0xAA));
}

TEST(SharedMemoryUserStreamWriterTest, WriteDataBoundaryPayloadSizes) {
  constexpr uint32_t kSlotCapacityBytes = 64;
  SharedMemoryUserStreamWriter writer(1, kSlotCapacityBytes);

  const base::ReadOnlySharedMemoryRegion read_only_region =
      writer.DuplicateRegion();
  const base::ReadOnlySharedMemoryMapping read_mapping = read_only_region.Map();
  ASSERT_TRUE(read_mapping.IsValid());
  const internal::SharedMemoryUserStreamHeader* const header =
      read_mapping.GetMemoryAs<internal::SharedMemoryUserStreamHeader>();
  ASSERT_NE(header, nullptr);

  const uint32_t initial_slot_header_offset = header->active_slot_header_offset;
  EXPECT_EQ(initial_slot_header_offset, internal::kNoActiveSlotHeaderOffset);

  // Write zero bytes.
  ASSERT_TRUE(writer.WriteData(
      [](base::span<uint8_t> data) -> std::optional<size_t> { return 0; }));
  const uint32_t first_write_header_offset = header->active_slot_header_offset;
  EXPECT_NE(first_write_header_offset, initial_slot_header_offset);
  const internal::SharedMemoryUserStreamSlotHeader* const first_write_header =
      GetSlotHeader(read_mapping, first_write_header_offset);
  ASSERT_NE(first_write_header, nullptr);
  EXPECT_EQ(first_write_header->generation, 2u);
  EXPECT_EQ(first_write_header->payload_size, 0u);

  // Write exactly slot size.
  ASSERT_TRUE(writer.WriteData(
      [kSlotCapacityBytes](base::span<uint8_t> data) -> std::optional<size_t> {
        std::ranges::fill(data, 0xFF);
        return kSlotCapacityBytes;
      }));
  const uint32_t second_write_header_offset = header->active_slot_header_offset;
  EXPECT_NE(second_write_header_offset, first_write_header_offset);
  EXPECT_NE(second_write_header_offset, initial_slot_header_offset);
  const internal::SharedMemoryUserStreamSlotHeader* const second_write_header =
      GetSlotHeader(read_mapping, second_write_header_offset);
  ASSERT_NE(second_write_header, nullptr);
  EXPECT_EQ(second_write_header->generation, 2u);
  EXPECT_EQ(second_write_header->payload_size, kSlotCapacityBytes);
}

TEST(SharedMemoryUserStreamWriterTest, GenerationCounterAllowsRaceDetection) {
  constexpr uint32_t kSlotCapacityBytes = 64;
  SharedMemoryUserStreamWriter writer(1, kSlotCapacityBytes);

  const base::ReadOnlySharedMemoryRegion read_only_region =
      writer.DuplicateRegion();
  const base::ReadOnlySharedMemoryMapping read_mapping = read_only_region.Map();
  ASSERT_TRUE(read_mapping.IsValid());
  const internal::SharedMemoryUserStreamHeader* const header =
      read_mapping.GetMemoryAs<internal::SharedMemoryUserStreamHeader>();
  ASSERT_NE(header, nullptr);

  // Step 1: Writer populates Slot A. Record generation as the reader's initial
  // snapshot.
  ASSERT_TRUE(
      writer.WriteData([](base::span<uint8_t> data) -> std::optional<size_t> {
        std::ranges::fill(data, 0xAA);
        return 16;
      }));
  const uint32_t slot_a_header_offset = header->active_slot_header_offset;
  const uint32_t initial_gen_a =
      GetSlotHeader(read_mapping, slot_a_header_offset)->generation;
  EXPECT_EQ(initial_gen_a, 2u);
  EXPECT_EQ(initial_gen_a & 1u, 0u);

  // Step 2: Writer populates Slot B. Slot A is now the inactive slot.
  ASSERT_TRUE(
      writer.WriteData([](base::span<uint8_t> data) -> std::optional<size_t> {
        std::ranges::fill(data, 0xBB);
        return 16;
      }));
  const uint32_t slot_b_header_offset = header->active_slot_header_offset;
  EXPECT_NE(slot_b_header_offset, slot_a_header_offset);

  // Step 3: Writer updates Slot A again. Intercept generation mid-write to
  // verify that a concurrent reader sees the odd (dirty) generation bit.
  bool race_detected_mid_write = false;
  ASSERT_TRUE(
      writer.WriteData([&](base::span<uint8_t> data) -> std::optional<size_t> {
        const uint32_t mid_write_gen_a =
            GetSlotHeader(read_mapping, slot_a_header_offset)->generation;
        EXPECT_EQ(mid_write_gen_a, 3u);
        EXPECT_NE(mid_write_gen_a, initial_gen_a);
        EXPECT_EQ(mid_write_gen_a & 1u, 1u);
        if (mid_write_gen_a != initial_gen_a || (mid_write_gen_a & 1u)) {
          race_detected_mid_write = true;
        }
        std::ranges::fill(data, 0xCC);
        return 16;
      }));
  EXPECT_TRUE(race_detected_mid_write);

  // Step 4: After write completes, reader verifies generation changed from
  // initial snapshot.
  const uint32_t post_write_gen_a =
      GetSlotHeader(read_mapping, slot_a_header_offset)->generation;
  EXPECT_EQ(post_write_gen_a, 4u);
  EXPECT_NE(post_write_gen_a, initial_gen_a);
}

}  // namespace
}  // namespace crash_reporter
