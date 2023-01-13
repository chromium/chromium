// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_STREAM_BUFFER_MANAGER_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_STREAM_BUFFER_MANAGER_H_

#include <memory>
#include <unordered_map>

#include "third_party/nearby/src/connections/payload.h"
#include "third_party/nearby/src/internal/platform/byte_array.h"

namespace nearby {
namespace connections {

// Manages payloads with type "stream" received over Nearby Connections. Streams
// over a certain size are delivered in chunks and need to be reassembled upon
// completion.
//
// Clients should start tracking a payload via StartTrackingPayload(). When
// more bytes have been transferred, clients should invoke
// HandleBytesTransferred(), passing the cumulative number of bytes that have
// been transferred. When all bytes have finished being transferred, clients
// should invoke GetCompletePayloadAndStopTracking() to get the complete,
// reassembled payload.
//
// If a payload has failed or been canceled, clients should invoke
// StopTrackingFailedPayload() so that this class can clean up its internal
// buffer.
class NearbyConnectionsStreamBufferManager {
 public:
  NearbyConnectionsStreamBufferManager();
  ~NearbyConnectionsStreamBufferManager();

  // Starts tracking the given payload.
  void StartTrackingPayload(Payload payload);

  // Returns whether a payload with the provided ID is being tracked.
  bool IsTrackingPayload(int64_t payload_id) const;

  // Stops tracking the payload with the provided ID and cleans up internal
  // memory being used to buffer the partially-completed transfer.
  void StopTrackingFailedPayload(int64_t payload_id);

  // Processes incoming bytes by reading from the input stream.
  void HandleBytesTransferred(int64_t payload_id,
                              int64_t cumulative_bytes_transferred_so_far);

  // Returns the completed buffer and deletes internal buffers.
  ByteArray GetCompletePayloadAndStopTracking(int64_t payload_id);

 private:
  struct PayloadWithBuffer {
    explicit PayloadWithBuffer(Payload payload);

    Payload payload;

    // Partially-complete buffer which contains the bytes which have been read
    // up to this point.
    std::string buffer;
  };

  std::unordered_map<int64_t, std::unique_ptr<PayloadWithBuffer>>
      id_to_payload_with_buffer_map_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_STREAM_BUFFER_MANAGER_H_
