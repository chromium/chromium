// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections_stream_buffer_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "components/cross_device/logging/logging.h"
#include "third_party/nearby/src/internal/platform/exception.h"
#include "third_party/nearby/src/internal/platform/input_stream.h"

namespace nearby {
namespace connections {

NearbyConnectionsStreamBufferManager::PayloadWithBuffer::PayloadWithBuffer(
    Payload payload)
    : payload(std::move(payload)) {}

NearbyConnectionsStreamBufferManager::NearbyConnectionsStreamBufferManager() =
    default;

NearbyConnectionsStreamBufferManager::~NearbyConnectionsStreamBufferManager() =
    default;

void NearbyConnectionsStreamBufferManager::StartTrackingPayload(
    Payload payload) {
  int64_t payload_id = payload.GetId();
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << "Starting to track stream payload with ID " << payload_id;

  id_to_payload_with_buffer_map_[payload_id] =
      std::make_unique<PayloadWithBuffer>(std::move(payload));
}

bool NearbyConnectionsStreamBufferManager::IsTrackingPayload(
    int64_t payload_id) const {
  return base::Contains(id_to_payload_with_buffer_map_, payload_id);
}

void NearbyConnectionsStreamBufferManager::StopTrackingFailedPayload(
    int64_t payload_id) {
  id_to_payload_with_buffer_map_.erase(payload_id);
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << "Stopped tracking payload with ID " << payload_id << " "
      << "and cleared internal memory.";
}

void NearbyConnectionsStreamBufferManager::HandleBytesTransferred(
    int64_t payload_id,
    int64_t cumulative_bytes_transferred_so_far) {
  auto it = id_to_payload_with_buffer_map_.find(payload_id);
  if (it == id_to_payload_with_buffer_map_.end()) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << "Attempted to handle stream bytes for payload with ID " << payload_id
        << ", but this payload was not being tracked.";
    return;
  }

  PayloadWithBuffer* payload_with_buffer = it->second.get();

  // We only need to read the new bytes which have not already been inserted
  // into the buffer.
  size_t bytes_to_read =
      cumulative_bytes_transferred_so_far - payload_with_buffer->buffer.size();

  InputStream* stream = payload_with_buffer->payload.AsStream();
  if (!stream) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << "Payload with ID " << payload_id << " is not a stream "
        << "payload; transfer has failed.";
    StopTrackingFailedPayload(payload_id);
    return;
  }

  ExceptionOr<ByteArray> bytes = stream->Read(bytes_to_read);
  if (!bytes.ok()) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << "Payload with ID " << payload_id << " encountered "
        << "exception while reading; transfer has failed.";
    StopTrackingFailedPayload(payload_id);
    return;
  }

  payload_with_buffer->buffer += static_cast<std::string>(bytes.result());
}

ByteArray
NearbyConnectionsStreamBufferManager::GetCompletePayloadAndStopTracking(
    int64_t payload_id) {
  auto it = id_to_payload_with_buffer_map_.find(payload_id);
  if (it == id_to_payload_with_buffer_map_.end()) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << "Attempted to get complete payload with ID " << payload_id
        << ", but this payload was not being tracked.";
    return ByteArray();
  }

  ByteArray complete_payload(it->second->buffer);

  // Close stream and erase internal state before returning payload.
  it->second->payload.AsStream()->Close();
  id_to_payload_with_buffer_map_.erase(it);

  return complete_payload;
}

}  // namespace connections
}  // namespace nearby
