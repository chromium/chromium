// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/shared_memory_user_stream_reader.h"

#include <stdint.h>

#include <atomic>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/aligned_memory.h"
#include "base/numerics/checked_math.h"
#include "base/threading/platform_thread.h"
#include "base/types/expected.h"
#include "components/crash/core/common/shared_memory_user_stream.h"

namespace crash_reporter {

namespace {

// Reads a 32-bit field atomically from read-only shared memory.
uint32_t AtomicLoad(const uint32_t& val) {
  return std::atomic_ref<uint32_t>(const_cast<uint32_t&>(val))
      .load(std::memory_order_acquire);
}

// Validates the envelope header (magic, version, and alignment).
const internal::SharedMemoryUserStreamHeader* ValidateAndGetHeader(
    base::span<const uint8_t> user_stream_buffer) {
  if (user_stream_buffer.size() <
          sizeof(internal::SharedMemoryUserStreamHeader) ||
      !base::IsAligned(user_stream_buffer.data(),
                       alignof(internal::SharedMemoryUserStreamHeader))) {
    return nullptr;
  }

  const auto* header =
      reinterpret_cast<const internal::SharedMemoryUserStreamHeader*>(
          user_stream_buffer.data());
  if (header->magic != internal::kUserStreamMagic ||
      header->version != internal::kUserStreamVersion) {
    return nullptr;
  }

  return header;
}

// Validates that `active_slot_header_offset` points to a valid, aligned slot
// header within `user_stream_buffer` and is not the
// `kNoActiveSlotHeaderOffset` sentinel.
const internal::SharedMemoryUserStreamSlotHeader* ValidateAndGetSlotHeader(
    base::span<const uint8_t> user_stream_buffer,
    uint32_t active_slot_header_offset) {
  // `kNoActiveSlotHeaderOffset` indicates no stream data has been published
  // yet.
  if (active_slot_header_offset == internal::kNoActiveSlotHeaderOffset ||
      active_slot_header_offset <
          sizeof(internal::SharedMemoryUserStreamHeader) ||
      !base::IsAligned(active_slot_header_offset,
                       alignof(internal::SharedMemoryUserStreamSlotHeader))) {
    return nullptr;
  }

  size_t slot_header_end = 0;
  if (!base::CheckAdd(active_slot_header_offset,
                      sizeof(internal::SharedMemoryUserStreamSlotHeader))
           .AssignIfValid(&slot_header_end) ||
      slot_header_end > user_stream_buffer.size()) {
    return nullptr;
  }

  return reinterpret_cast<const internal::SharedMemoryUserStreamSlotHeader*>(
      user_stream_buffer.subspan(active_slot_header_offset).data());
}

// Reason why a single attempt to read a slot's payload failed.
enum class ReadSlotError {
  // A concurrent writer was actively modifying the slot (dirty generation or
  // generation counter changed during the read).
  kTransientConflict,

  // The slot contents or header fields are malformed or exceed buffer bounds.
  kCorruptData,
};

// Attempts a single lock-free seqlock read of the slot's payload.
//
// Returns a vector containing the copied payload on success.
// Returns `ReadSlotError::kTransientConflict` if the read encountered a
// concurrent modification (odd generation or generation changed during read).
// Returns `ReadSlotError::kCorruptData` if payload size bounds or arithmetic
// overflow occur while the slot is in a stable state.
base::expected<std::vector<uint8_t>, ReadSlotError> TryReadSlotPayload(
    base::span<const uint8_t> user_stream_buffer,
    const internal::SharedMemoryUserStreamSlotHeader* slot_header,
    uint32_t active_slot_header_offset) {
  // Readers must check `generation` before and after copying a slot to detect
  // concurrent modifications and retry.
  const uint32_t generation_before = AtomicLoad(slot_header->generation);
  if (generation_before & 1u) {
    // Odd value indicates an in-progress update ("dirty bit").
    return base::unexpected(ReadSlotError::kTransientConflict);
  }

  const uint32_t payload_size = slot_header->payload_size;

  size_t payload_start = 0;
  size_t payload_end = 0;
  if (!base::CheckAdd(active_slot_header_offset,
                      sizeof(internal::SharedMemoryUserStreamSlotHeader))
           .AssignIfValid(&payload_start) ||
      !base::CheckAdd(payload_start, payload_size)
           .AssignIfValid(&payload_end) ||
      payload_end > user_stream_buffer.size()) {
    // If the slot was modified concurrently during the read, treat invalid
    // bounds as a transient conflict rather than permanent corruption.
    const uint32_t current_generation = AtomicLoad(slot_header->generation);
    if (current_generation != generation_before) {
      return base::unexpected(ReadSlotError::kTransientConflict);
    }
    return base::unexpected(ReadSlotError::kCorruptData);
  }

  const base::span<const uint8_t> payload =
      user_stream_buffer.subspan(payload_start, payload_size);
  std::vector<uint8_t> payload_copy = base::ToVector(payload);

  const uint32_t generation_after = AtomicLoad(slot_header->generation);
  if (generation_after != generation_before) {
    // Slot was modified concurrently during the read.
    return base::unexpected(ReadSlotError::kTransientConflict);
  }

  return payload_copy;
}

}  // namespace

std::optional<UserStreamData> ExtractSharedMemoryUserStreamData(
    base::span<const uint8_t> user_stream_buffer) {
  const internal::SharedMemoryUserStreamHeader* header =
      ValidateAndGetHeader(user_stream_buffer);
  if (!header) {
    return std::nullopt;
  }

  // Retry loop to capture a consistent slot snapshot across concurrent updates.
  constexpr int kMaxAttempts = 10;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const uint32_t active_slot_header_offset =
        AtomicLoad(header->active_slot_header_offset);

    const internal::SharedMemoryUserStreamSlotHeader* slot_header =
        ValidateAndGetSlotHeader(user_stream_buffer, active_slot_header_offset);
    if (!slot_header) {
      return std::nullopt;
    }

    base::expected<std::vector<uint8_t>, ReadSlotError> payload =
        TryReadSlotPayload(user_stream_buffer, slot_header,
                           active_slot_header_offset);
    if (payload.has_value()) {
      return UserStreamData{
          .user_stream_type = header->user_stream_type,
          .payload = std::move(payload).value(),
      };
    }

    if (payload.error() == ReadSlotError::kCorruptData) {
      return std::nullopt;
    }

    if (attempt + 1 < kMaxAttempts) {
      base::PlatformThread::YieldCurrentThread();
    }
  }

  return std::nullopt;
}

}  // namespace crash_reporter
