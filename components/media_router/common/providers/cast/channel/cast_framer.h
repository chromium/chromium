// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_FRAMER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_FRAMER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "net/base/io_buffer.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {
using ::openscreen::cast::proto::CastMessage;

// Class for constructing and parsing CastMessage packet data.
class MessageFramer {
 public:
  using ChannelError = ::cast_channel::ChannelError;

  // |input_buffer|: The input buffer used by all socket read operations that
  //                 feed data into the framer.
  explicit MessageFramer(scoped_refptr<net::GrowableIOBuffer> input_buffer);

  MessageFramer(const MessageFramer&) = delete;
  MessageFramer& operator=(const MessageFramer&) = delete;

  ~MessageFramer();

  // The number of bytes required from |input_buffer| to complete the
  // CastMessage being read.
  // Returns zero if |error_| is true (framer is in an invalid state.)
  size_t BytesRequested();

  // Serializes |message_proto| into |message_data|.
  // Returns true if the message was serialized successfully, false otherwise.
  static bool Serialize(const CastMessage& message_proto,
                        std::string* message_data);

  // Reads bytes from |input_buffer_| and returns a new CastMessage if one
  // is fully read.
  //
  // |num_bytes| The number of bytes received by a read operation.
  //             Value must be <= BytesRequested().
  // |message_length| Size of the deserialized message object, in bytes. For
  //                  logging purposes. Set to zero if no message was parsed.
  // |error| The result of the ingest operation. Set to CHANNEL_ERROR_NONE
  //         if no error occurred.
  // Returns A pointer to a parsed CastMessage if a message was received
  //         in its entirety, nullptr otherwise.
  std::unique_ptr<CastMessage> Ingest(size_t num_bytes,
                                      size_t* message_length,
                                      ChannelError* error);

  // Message header struct. If fields are added, be sure to update
  // header_size().  Public to allow use of *_size() methods in unit tests.
  struct MessageHeader {
    MessageHeader();
    // Sets the message size.
    void SetMessageSize(size_t message_size);
    // Prepends this header to |str|.
    void PrependToString(std::string* str);
    // Reads |header| from the bytes specified by |data|.
    static void Deserialize(base::span<const uint8_t> data,
                            MessageHeader* header);
    // Maximum size (in bytes) of a message payload on the wire (does not
    // include header).
    static size_t max_body_size();
    // Maximum size (in bytes) of a message (header + payload) on the wire.
    static size_t max_message_size();
    std::string ToString();
    // The size of the following protocol message in bytes, in host byte order.
    uint32_t message_size = 0u;
  };
  static_assert(sizeof(MessageHeader) == sizeof(uint32_t));

 private:
  enum MessageElement { HEADER, BODY };

  // Prepares the framer for ingesting a new message.
  void Reset();

  // The element of the message that will be read on the next call to Ingest().
  MessageElement current_element_;

  // Total size of the message, in bytes (head + body).
  size_t message_bytes_received_;

  // Size of the body alone, in bytes.
  size_t body_size_;

  // Input buffer which carries message data read from the socket.
  // Caller is responsible for writing into this buffer.
  scoped_refptr<net::GrowableIOBuffer> input_buffer_;

  // Disables Ingest functionality is the parser receives invalid data.
  bool error_;
};
}  // namespace cast_channel
#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_FRAMER_H_
