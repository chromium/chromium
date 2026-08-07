// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/shared_memory_user_stream_writer.h"

#include <atomic>
#include <memory>
#include <optional>

#include "base/bits.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/aligned_memory.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "components/crash/core/common/shared_memory_user_stream.h"

namespace crash_reporter {

namespace {

constexpr size_t kMaxUserStreamSlotCapacityBytes = 4 * 1024 * 1024;

// Returns the size of a single data slot (header + payload), rounded up to
// match alignof(SharedMemoryUserStreamSlotHeader) so consecutive slot headers
// remain aligned.
constexpr uint32_t GetSlotSize(uint32_t slot_capacity_bytes) {
  const base::CheckedNumeric<uint32_t> unaligned_size = base::CheckAdd(
      sizeof(internal::SharedMemoryUserStreamSlotHeader), slot_capacity_bytes);
  return base::bits::AlignUp<uint32_t>(
      unaligned_size.ValueOrDie(),
      alignof(internal::SharedMemoryUserStreamSlotHeader));
}

// Returns the byte offset of the data slot header at `slot_index` within the
// shared memory buffer.
constexpr uint32_t GetSlotHeaderOffset(uint32_t slot_index,
                                       uint32_t slot_capacity_bytes) {
  CHECK_LE(slot_index, 1u);
  const base::CheckedNumeric<uint32_t> offset = base::CheckAdd(
      sizeof(internal::SharedMemoryUserStreamHeader),
      base::CheckMul(slot_index, GetSlotSize(slot_capacity_bytes)));
  return offset.ValueOrDie();
}

// Calculates the total shared memory buffer size required for the header and
// all data slots given `slot_capacity_bytes`.
constexpr size_t CalculateUserStreamBufferSize(uint32_t slot_capacity_bytes) {
  return base::CheckAdd(sizeof(internal::SharedMemoryUserStreamHeader),
                        base::CheckMul(2u, GetSlotSize(slot_capacity_bytes)))
      .ValueOrDie();
}

// Returns a pointer to the slot header at `slot_header_offset` within
// `user_stream_buffer`.
internal::SharedMemoryUserStreamSlotHeader* GetSlotHeader(
    const base::span<uint8_t>& user_stream_buffer,
    uint32_t slot_header_offset) {
  auto* const slot_header =
      reinterpret_cast<internal::SharedMemoryUserStreamSlotHeader*>(
          user_stream_buffer.subspan(slot_header_offset).data());
  return slot_header;
}

}  // namespace

SharedMemoryUserStreamWriter::SharedMemoryUserStreamWriter(
    uint32_t user_stream_type,
    uint32_t slot_capacity_bytes)
    : slot_capacity_bytes_(slot_capacity_bytes) {
  CHECK_GT(slot_capacity_bytes, 0u);
  CHECK_LE(slot_capacity_bytes, kMaxUserStreamSlotCapacityBytes);

  const size_t buffer_size =
      CalculateUserStreamBufferSize(slot_capacity_bytes_);

  // Eagerly validate slot header offsets during construction.
  GetSlotHeaderOffset(0, slot_capacity_bytes_);
  GetSlotHeaderOffset(1, slot_capacity_bytes_);

  mapped_region_ = base::ReadOnlySharedMemoryRegion::Create(buffer_size);
  CHECK(mapped_region_.IsValid());

  internal::SharedMemoryUserStreamHeader* const header =
      mapped_region_.mapping
          .GetMemoryAs<internal::SharedMemoryUserStreamHeader>();
  CHECK(header);
  CHECK(
      base::IsAligned(header, alignof(internal::SharedMemoryUserStreamHeader)));

  *header = {
      .magic = internal::kUserStreamMagic,
      .version = internal::kUserStreamVersion,
      .user_stream_type = user_stream_type,
      .active_slot_header_offset = internal::kNoActiveSlotHeaderOffset,
  };
}

bool SharedMemoryUserStreamWriter::WriteData(
    base::FunctionRef<std::optional<size_t>(base::span<uint8_t>)> producer) {
  const uint32_t new_active_slot_header_offset =
      GetSlotHeaderOffset(inactive_slot_index_, slot_capacity_bytes_);

  const base::span<uint8_t> user_stream_buffer =
      mapped_region_.mapping.GetMemoryAsSpan<uint8_t>();
  internal::SharedMemoryUserStreamSlotHeader* const slot_header =
      GetSlotHeader(user_stream_buffer, new_active_slot_header_offset);
  std::atomic_ref<uint32_t> atomic_generation(slot_header->generation);

  // Increment the generation counter before beginning an update (setting the
  // low bit to 1, indicating a "dirty" state).
  atomic_generation.fetch_add(1, std::memory_order_acq_rel);

  const base::span<uint8_t> slot_payload_span = user_stream_buffer.subspan(
      new_active_slot_header_offset +
          sizeof(internal::SharedMemoryUserStreamSlotHeader),
      slot_capacity_bytes_);

  const std::optional<size_t> bytes_written = producer(slot_payload_span);
  if (!bytes_written) {
    // Restore the generation counter to an even ("clean") value on failure so
    // that out-of-process readers can still detect concurrent modification if
    // this slot was previously active.
    atomic_generation.fetch_add(1, std::memory_order_release);
    return false;
  }
  CHECK_LE(bytes_written.value(), slot_payload_span.size());

  slot_header->payload_size =
      base::checked_cast<uint32_t>(bytes_written.value());

  // Increment the generation counter after completing an update (setting the
  // low bit to 0, indicating a "clean" state).
  atomic_generation.fetch_add(1, std::memory_order_release);

  internal::SharedMemoryUserStreamHeader* const header =
      mapped_region_.mapping
          .GetMemoryAs<internal::SharedMemoryUserStreamHeader>();
  CHECK(header);

  std::atomic_ref<uint32_t> atomic_active_slot_header_offset(
      header->active_slot_header_offset);
  atomic_active_slot_header_offset.store(new_active_slot_header_offset,
                                         std::memory_order_release);

  inactive_slot_index_ = 1u - inactive_slot_index_;
  return true;
}

base::ReadOnlySharedMemoryRegion SharedMemoryUserStreamWriter::DuplicateRegion()
    const {
  return mapped_region_.region.Duplicate();
}

}  // namespace crash_reporter
