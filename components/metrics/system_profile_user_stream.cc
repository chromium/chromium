// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/system_profile_user_stream.h"

#include <stdint.h>

#include <optional>

#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "components/crash/core/common/shared_memory_user_stream_writer.h"

namespace metrics {

namespace {

// The stream type assigned to the minidump stream that holds the serialized
// system profile proto.
constexpr uint32_t kSystemProfileStreamType = 0x4B6B0003;

}  // namespace

// static
SystemProfileUserStream& SystemProfileUserStream::Get() {
  static base::NoDestructor<SystemProfileUserStream> instance;
  return *instance;
}

SystemProfileUserStream::SystemProfileUserStream() = default;

SystemProfileUserStream::~SystemProfileUserStream() = default;

void SystemProfileUserStream::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user_stream_writer_) {
    user_stream_writer_ =
        std::make_unique<crash_reporter::SharedMemoryUserStreamWriter>(
            kSystemProfileStreamType, kSystemProfileSlotCapacityBytes);
  }
}

base::ReadOnlySharedMemoryRegion
SystemProfileUserStream::DuplicateSharedMemoryRegion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user_stream_writer_) {
    return base::ReadOnlySharedMemoryRegion();
  }
  return user_stream_writer_->DuplicateRegion();
}

void SystemProfileUserStream::WritePayload(std::string_view payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user_stream_writer_) {
    return;
  }

  if (payload.size() > kSystemProfileSlotCapacityBytes) {
    // If the payload exceeds the slot capacity, returning early is necessary
    // because `copy_prefix_from` would CHECK fail and crash the process if the
    // source span is larger than the destination span. Furthermore, partially
    // written Protobufs are often considered corrupted by the Crash server and
    // may be discarded anyway.
    return;
  }

  user_stream_writer_->WriteData(
      [&payload](base::span<uint8_t> slot_span) -> std::optional<size_t> {
        slot_span.copy_prefix_from(base::as_byte_span(payload));
        return payload.size();
      });

  // TODO(crbug.com/514425492): Record UMA histograms for write success
  // and overflow.
}

}  // namespace metrics
