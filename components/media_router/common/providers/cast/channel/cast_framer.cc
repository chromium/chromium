// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/media_router/common/providers/cast/channel/cast_framer.h"

#include <stdlib.h>

#include <limits>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {
MessageFramer::MessageFramer(scoped_refptr<net::GrowableIOBuffer> input_buffer)
    : input_buffer_(input_buffer), error_(false) {
  Reset();
}

MessageFramer::~MessageFramer() = default;

MessageFramer::MessageHeader::MessageHeader() = default;

void MessageFramer::MessageHeader::SetMessageSize(size_t size) {
  CHECK_GT(size, 0u);
  message_size = base::checked_cast<uint32_t>(size);
}

// TODO(mfoltz): Investigate replacing header serialization with base::Pickle,
// if bit-for-bit compatible.
void MessageFramer::MessageHeader::PrependToString(std::string* str) {
  std::array<uint8_t, sizeof(message_size)> bytes =
      base::U32ToBigEndian(message_size);
  str->insert(str->begin(), bytes.begin(), bytes.end());
}

void MessageFramer::MessageHeader::Deserialize(base::span<const uint8_t> data,
                                               MessageHeader* header) {
  header->message_size =
      base::U32FromBigEndian(data.first<sizeof(header->message_size)>());
}

// static
size_t MessageFramer::MessageHeader::max_body_size() {
  return 65536;
}

// static
size_t MessageFramer::MessageHeader::max_message_size() {
  return sizeof(MessageHeader) + max_body_size();
}

std::string MessageFramer::MessageHeader::ToString() {
  return "{message_size: " + base::NumberToString(message_size) + "}";
}

// static
bool MessageFramer::Serialize(const CastMessage& message_proto,
                              std::string* message_data) {
  DCHECK(message_data);
  message_proto.SerializeToString(message_data);
  size_t message_size = message_data->size();
  if (message_size > MessageHeader::max_body_size()) {
    message_data->clear();
    return false;
  }
  MessageHeader header;
  header.SetMessageSize(message_size);
  header.PrependToString(message_data);
  return true;
}

size_t MessageFramer::BytesRequested() {
  size_t bytes_left;
  if (error_) {
    return 0;
  }

  switch (current_element_) {
    case HEADER:
      bytes_left = sizeof(MessageHeader) - message_bytes_received_;
      DCHECK_LE(bytes_left, sizeof(MessageHeader));
      VLOG(2) << "Bytes needed for header: " << bytes_left;
      return bytes_left;
    case BODY:
      bytes_left =
          (body_size_ + sizeof(MessageHeader)) - message_bytes_received_;
      DCHECK_LE(bytes_left, MessageHeader::max_body_size());
      VLOG(2) << "Bytes needed for body: " << bytes_left;
      return bytes_left;
    default:
      NOTREACHED_IN_MIGRATION() << "Unhandled packet element type.";
      return 0;
  }
}

std::unique_ptr<CastMessage> MessageFramer::Ingest(size_t num_bytes,
                                                   size_t* message_length,
                                                   ChannelError* error) {
  DCHECK(error);
  DCHECK(message_length);
  if (error_) {
    *error = ChannelError::INVALID_MESSAGE;
    return nullptr;
  }

  DCHECK_EQ(base::checked_cast<int32_t>(message_bytes_received_),
            input_buffer_->offset());
  CHECK_LE(num_bytes, BytesRequested());
  message_bytes_received_ += num_bytes;
  *error = ChannelError::NONE;
  *message_length = 0;
  switch (current_element_) {
    case HEADER:
      if (BytesRequested() == 0) {
        MessageHeader header;
        MessageHeader::Deserialize(input_buffer_->everything(), &header);
        if (header.message_size > MessageHeader::max_body_size()) {
          VLOG(1) << "Error parsing header (message size too large).";
          *error = ChannelError::INVALID_MESSAGE;
          error_ = true;
          return nullptr;
        }
        current_element_ = BODY;
        body_size_ = header.message_size;
      }
      break;
    case BODY:
      if (BytesRequested() == 0) {
        std::unique_ptr<CastMessage> parsed_message(new CastMessage);
        base::span<const uint8_t> data = input_buffer_->everything().subspan(
            sizeof(MessageHeader), body_size_);
        if (!parsed_message->ParseFromArray(data.data(), data.size())) {
          VLOG(1) << "Error parsing packet body.";
          *error = ChannelError::INVALID_MESSAGE;
          error_ = true;
          return nullptr;
        }
        *message_length = body_size_;
        Reset();
        return parsed_message;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unhandled packet element type.";
      return nullptr;
  }

  input_buffer_->set_offset(message_bytes_received_);
  return nullptr;
}

void MessageFramer::Reset() {
  current_element_ = HEADER;
  message_bytes_received_ = 0;
  body_size_ = 0;
  input_buffer_->set_offset(0);
}

}  // namespace cast_channel
