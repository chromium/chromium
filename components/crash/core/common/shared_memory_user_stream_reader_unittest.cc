// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/shared_memory_user_stream_reader.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/crash/core/common/shared_memory_user_stream_writer.h"
#include "components/crash/core/common/shared_memory_user_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter {
namespace {

constexpr uint32_t kDefaultUserStreamType = 123;
constexpr uint32_t kDefaultSlotCapacityBytes = 128;

class SharedMemoryUserStreamReaderTest : public testing::Test {
 protected:
  // Creates a mapped shared memory buffer populated with a valid header and
  // payload.
  std::vector<uint8_t> CreateValidBuffer(
      base::span<const uint8_t> payload = {},
      uint32_t user_stream_type = kDefaultUserStreamType,
      uint32_t slot_capacity_bytes = kDefaultSlotCapacityBytes) {
    SharedMemoryUserStreamWriter writer(user_stream_type, slot_capacity_bytes);

    EXPECT_TRUE(writer.WriteData(
        [&](base::span<uint8_t> data) -> std::optional<size_t> {
          std::ranges::copy(payload, data.begin());
          return payload.size();
        }));

    const base::ReadOnlySharedMemoryRegion region = writer.DuplicateRegion();
    const base::ReadOnlySharedMemoryMapping mapping = region.Map();
    EXPECT_TRUE(mapping.IsValid());
    if (!mapping.IsValid()) {
      return {};
    }

    const auto span = mapping.GetMemoryAsSpan<const uint8_t>();
    return std::vector<uint8_t>(span.begin(), span.end());
  }

  static internal::SharedMemoryUserStreamHeader* GetHeader(
      std::vector<uint8_t>& user_stream_buffer) {
    if (user_stream_buffer.size() <
        sizeof(internal::SharedMemoryUserStreamHeader)) {
      return nullptr;
    }
    return UNSAFE_BUFFERS(
        reinterpret_cast<internal::SharedMemoryUserStreamHeader*>(
            user_stream_buffer.data()));
  }

  static internal::SharedMemoryUserStreamSlotHeader* GetSlotHeader(
      std::vector<uint8_t>& user_stream_buffer,
      uint32_t slot_header_offset) {
    if (slot_header_offset +
            sizeof(internal::SharedMemoryUserStreamSlotHeader) >
        user_stream_buffer.size()) {
      return nullptr;
    }
    return UNSAFE_BUFFERS(
        reinterpret_cast<internal::SharedMemoryUserStreamSlotHeader*>(
            base::span(user_stream_buffer).subspan(slot_header_offset).data()));
  }
};

TEST_F(SharedMemoryUserStreamReaderTest, ExtractValidData) {
  const std::vector<uint8_t> expected_data = {0x01, 0x02, 0x03, 0x04, 0x05};
  std::vector<uint8_t> buffer = CreateValidBuffer(expected_data);

  std::optional<UserStreamData> stream_data =
      ExtractSharedMemoryUserStreamData(buffer);
  ASSERT_TRUE(stream_data.has_value());
  EXPECT_EQ(stream_data->user_stream_type, kDefaultUserStreamType);
  EXPECT_EQ(stream_data->payload, expected_data);
}

TEST_F(SharedMemoryUserStreamReaderTest, ExtractHandlesZeroBytePayload) {
  std::vector<uint8_t> buffer = CreateValidBuffer();

  std::optional<UserStreamData> stream_data =
      ExtractSharedMemoryUserStreamData(buffer);
  ASSERT_TRUE(stream_data.has_value());
  EXPECT_EQ(stream_data->user_stream_type, kDefaultUserStreamType);
  EXPECT_TRUE(stream_data->payload.empty());
}

TEST_F(SharedMemoryUserStreamReaderTest, ExtractRejectsInvalidBuffer) {
  // Empty buffer
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(base::span<const uint8_t>())
                   .has_value());

  // Buffer smaller than header
  std::vector<uint8_t> small_buffer(
      sizeof(internal::SharedMemoryUserStreamHeader) - 1, 0);
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(small_buffer).has_value());
}

TEST_F(SharedMemoryUserStreamReaderTest,
       ExtractReturnsNulloptWhenNoActiveSlot) {
  const SharedMemoryUserStreamWriter writer(kDefaultUserStreamType,
                                            kDefaultSlotCapacityBytes);

  const base::ReadOnlySharedMemoryRegion region = writer.DuplicateRegion();
  const base::ReadOnlySharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());

  // No WriteData() called yet -> active_slot_header_offset is
  // kNoActiveSlotHeaderOffset.
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(
                   mapping.GetMemoryAsSpan<const uint8_t>())
                   .has_value());
}

TEST_F(SharedMemoryUserStreamReaderTest, ExtractRejectsInvalidMagicOrVersion) {
  std::vector<uint8_t> buffer = CreateValidBuffer({1, 2, 3});
  auto* const header = GetHeader(buffer);
  ASSERT_NE(header, nullptr);

  // Invalid magic
  header->magic = 0xDEADBEEF;
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(buffer).has_value());

  // Invalid version
  header->magic = internal::kUserStreamMagic;
  header->version = 999;
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(buffer).has_value());
}

TEST_F(SharedMemoryUserStreamReaderTest, ExtractRejectsCorruptPayloadSize) {
  std::vector<uint8_t> buffer = CreateValidBuffer({1, 2, 3});
  auto* const header = GetHeader(buffer);
  ASSERT_NE(header, nullptr);
  auto* const slot_header =
      GetSlotHeader(buffer, header->active_slot_header_offset);
  ASSERT_NE(slot_header, nullptr);

  // Set an impossible payload size that exceeds the buffer.
  slot_header->payload_size = 0xFFFFFFFF;
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(buffer).has_value());
}

TEST_F(SharedMemoryUserStreamReaderTest,
       ExtractRejectsCorruptActiveSlotHeaderOffset) {
  std::vector<uint8_t> buffer = CreateValidBuffer({1, 2, 3});
  auto* const header = GetHeader(buffer);
  ASSERT_NE(header, nullptr);

  // Offset that causes arithmetic overflow when computing slot header end.
  header->active_slot_header_offset = 0xFFFFFFF0;
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(buffer).has_value());

  // Offset out of bounds.
  header->active_slot_header_offset =
      static_cast<uint32_t>(buffer.size() + 1024);
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(buffer).has_value());
}

TEST_F(SharedMemoryUserStreamReaderTest, ExtractRejectsDirtySlotGeneration) {
  std::vector<uint8_t> buffer = CreateValidBuffer({1, 2, 3});
  auto* const header = GetHeader(buffer);
  ASSERT_NE(header, nullptr);
  auto* const slot_header =
      GetSlotHeader(buffer, header->active_slot_header_offset);
  ASSERT_NE(slot_header, nullptr);

  // Set odd generation (in-progress/dirty bit).
  slot_header->generation = 1;
  EXPECT_FALSE(ExtractSharedMemoryUserStreamData(buffer).has_value());
}

}  // namespace
}  // namespace crash_reporter
