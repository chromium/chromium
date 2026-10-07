// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/shared_memory_user_stream_mach_port.h"

#include <mach/mach.h>

#include <vector>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/check_op.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"

namespace crash_reporter::internal {

namespace {

constexpr mach_msg_id_t kSharedMemoryUserStreamMsgId = 'SMUS';

// Arbitrary maximum number of user streams to transmit, for sizing the
// receiver's buffer. Increase this as needed.
constexpr size_t kMaxUserStreams = 8;

// Returns the size of a Mach message containing the underlying Mach ports and
// sizes of `num_streams` shared-memory user streams.
constexpr size_t GetMessageSize(size_t num_streams) {
  // Basic Mach message + list of ports + list of shared-memory-region sizes.
  return sizeof(mach_msg_base_t) +
         num_streams * sizeof(mach_msg_port_descriptor_t) +
         num_streams * sizeof(uint64_t);
}

}  // namespace

base::apple::ScopedMachReceiveRight SendSharedMemoryUserStreamsToPort(
    std::vector<base::ReadOnlySharedMemoryRegion> user_streams) {
  if (user_streams.empty()) {
    return {};
  }

  // Create a Mach receive right.
  base::apple::ScopedMachReceiveRight receive_right;
  kern_return_t kr = mach_port_allocate(
      mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
      base::apple::ScopedMachReceiveRight::Receiver(receive_right).get());
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_allocate port";
    return {};
  }

  // Get the corresponding send right.
  mach_port_t send_right = MACH_PORT_NULL;
  mach_msg_type_name_t send_right_type = 0;
  kr = mach_port_extract_right(mach_task_self(), receive_right.get(),
                               MACH_MSG_TYPE_MAKE_SEND, &send_right,
                               &send_right_type);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_extract_right send_right";
    return {};
  }
  base::apple::ScopedMachSendRight scoped_send_right(send_right);

  // Create a Mach message to transmit the underlying Mach port and size of each
  // shared-memory region in `user_streams`.
  const size_t num_streams = user_streams.size();
  CHECK_LE(num_streams, kMaxUserStreams)
      << "More shared-memory user streams than expected; increase "
         "kMaxUserStreams in shared_memory_user_stream_mach_port.cc.";
  const size_t message_size = GetMessageSize(num_streams);
  auto buffer = base::HeapArray<uint8_t>::WithSize(message_size);
  base::BufferIterator<uint8_t> iterator(buffer);

  auto* message = iterator.MutableObject<mach_msg_base_t>();
  message->header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND) | MACH_MSGH_BITS_COMPLEX;
  message->header.msgh_size = base::checked_cast<mach_msg_size_t>(message_size);
  message->header.msgh_remote_port = scoped_send_right.release();
  message->header.msgh_local_port = MACH_PORT_NULL;
  message->header.msgh_id = kSharedMemoryUserStreamMsgId;
  message->body.msgh_descriptor_count =
      base::checked_cast<mach_msg_size_t>(num_streams);

  // `descriptors` contains each region's underlying Mach port.
  auto descriptors =
      iterator.MutableSpan<mach_msg_port_descriptor_t>(num_streams);
  for (size_t i = 0; i < num_streams; ++i) {
    descriptors[i].name = user_streams[i].IsValid()
                              ? user_streams[i].GetPlatformHandle()
                              : MACH_PORT_NULL;
    descriptors[i].disposition = MACH_MSG_TYPE_COPY_SEND;
    descriptors[i].type = MACH_MSG_PORT_DESCRIPTOR;
  }
  // `sizes` contains each region's size.
  for (size_t i = 0; i < num_streams; ++i) {
    auto size = iterator.MutableSpan<uint8_t, 8>();
    size->copy_from(base::U64ToNativeEndian(
        user_streams[i].IsValid() ? user_streams[i].GetSize() : 0));
  }

  // Send the message and return the receive right that will allow the Crashpad
  // process to receive it.
  mach_msg_return_t mr =
      mach_msg(&message->header, MACH_SEND_MSG, message->header.msgh_size, 0,
               MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (mr != MACH_MSG_SUCCESS) {
    MACH_LOG(ERROR, mr) << "mach_msg send user streams";
    return {};
  }
  return receive_right;
}

std::vector<base::ReadOnlySharedMemoryRegion>
ReceiveSharedMemoryUserStreamsFromPort(
    base::apple::ScopedMachReceiveRight port) {
  if (!port.is_valid()) {
    return {};
  }

  // Create a buffer large enough to hold `kMaxUserStreams` streams.
  constexpr size_t kBufferSize =
      GetMessageSize(kMaxUserStreams) + sizeof(mach_msg_max_trailer_t);
  auto buffer = base::HeapArray<uint8_t>::WithSize(kBufferSize);
  auto* header = reinterpret_cast<mach_msg_header_t*>(buffer.data());

  // Receive a message from `port` and validate its message ID and size.
  mach_msg_return_t mr =
      mach_msg(header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
               base::checked_cast<mach_msg_size_t>(kBufferSize), port.get(), 0,
               MACH_PORT_NULL);
  if (mr != MACH_MSG_SUCCESS) {
    if (mr != MACH_RCV_TIMED_OUT) {
      MACH_LOG(ERROR, mr) << "mach_msg receive user streams";
    }
    return {};
  }
  base::BufferIterator<uint8_t> iterator(buffer);
  auto* message = iterator.Object<mach_msg_base_t>();
  if (!message || message->header.msgh_id != kSharedMemoryUserStreamMsgId ||
      !(message->header.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
    mach_msg_destroy(header);
    return {};
  }
  const size_t num_streams = message->body.msgh_descriptor_count;
  if (num_streams > kMaxUserStreams) {
    mach_msg_destroy(header);
    return {};
  }
  const size_t expected_size = GetMessageSize(num_streams);
  if (message->header.msgh_size < expected_size) {
    mach_msg_destroy(header);
    return {};
  }

  // Parse shared-memory regions' underlying Mach ports and sizes from
  // `descriptors` and `sizes`, respectively.
  std::vector<mach_msg_port_descriptor_t> descriptors;
  descriptors.reserve(num_streams);
  for (size_t i = 0; i < num_streams; ++i) {
    auto descriptor = iterator.CopyObject<mach_msg_port_descriptor_t>();
    if (!descriptor || descriptor->type != MACH_MSG_PORT_DESCRIPTOR) {
      mach_msg_destroy(header);
      return {};
    }
    descriptors.push_back(*descriptor);
  }
  std::vector<uint64_t> sizes;
  sizes.reserve(num_streams);
  for (size_t i = 0; i < num_streams; ++i) {
    auto size = iterator.CopyObject<uint64_t>();
    if (!size) {
      mach_msg_destroy(header);
      return {};
    }
    sizes.push_back(*size);
  }

  // Recreate a shared-memory region for each of the received ports and sizes.
  std::vector<base::ReadOnlySharedMemoryRegion> regions;
  regions.reserve(num_streams);
  for (size_t i = 0; i < num_streams; ++i) {
    auto platform_region = base::subtle::PlatformSharedMemoryRegion::TakeOrFail(
        base::apple::ScopedMachSendRight(descriptors[i].name),
        base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly, sizes[i],
        base::UnguessableToken::Create());
    if (!platform_region.has_value()) {
      continue;
    }
    auto region = base::ReadOnlySharedMemoryRegion::Deserialize(
        std::move(platform_region.value()));
    if (region.IsValid()) {
      regions.push_back(std::move(region));
    }
  }
  return regions;
}

}  // namespace crash_reporter::internal
