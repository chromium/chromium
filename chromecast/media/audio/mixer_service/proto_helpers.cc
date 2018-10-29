// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/proto_helpers.h"

#include <google/protobuf/message_lite.h>

#include <cstdint>
#include <cstring>

#include "base/big_endian.h"
#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/net/small_message_socket.h"
#include "net/base/io_buffer.h"

namespace chromecast {
namespace media {
namespace mixer_service {

scoped_refptr<net::IOBufferWithSize> SendProto(
    const google::protobuf::MessageLite& message,
    SmallMessageSocket* socket) {
  int16_t type = static_cast<int16_t>(MessageType::kMetadata);
  int message_size = message.ByteSize();
  int32_t padding_bytes = (4 - (message_size % 4)) % 4;

  int total_size =
      sizeof(type) + sizeof(padding_bytes) + message_size + padding_bytes;
  scoped_refptr<net::IOBufferWithSize> storage;
  void* buffer = socket->PrepareSend(total_size);
  char* ptr;
  if (buffer) {
    ptr = reinterpret_cast<char*>(buffer);
  } else {
    storage = base::MakeRefCounted<net::IOBufferWithSize>(sizeof(uint16_t) +
                                                          total_size);

    ptr = storage->data();
    base::WriteBigEndian(ptr, static_cast<uint16_t>(total_size));
    ptr += sizeof(uint16_t);
  }

  base::WriteBigEndian(ptr, type);
  ptr += sizeof(type);
  base::WriteBigEndian(ptr, padding_bytes);
  ptr += sizeof(padding_bytes);
  message.SerializeToArray(ptr, message_size);
  ptr += message_size;
  memset(ptr, 0, padding_bytes);

  if (buffer) {
    socket->Send();
  }
  return storage;
}

bool ReceiveProto(const char* data, int size, Generic* message) {
  int32_t padding_bytes;
  if (size < static_cast<int>(sizeof(padding_bytes))) {
    LOG(ERROR) << "Invalid metadata message size " << size;
    return false;
  }

  base::ReadBigEndian(data, &padding_bytes);
  data += sizeof(padding_bytes);
  size -= sizeof(padding_bytes);

  if (padding_bytes < 0 || padding_bytes > 3) {
    LOG(ERROR) << "Invalid padding bytes count: " << padding_bytes;
    return false;
  }

  if (size < padding_bytes) {
    LOG(ERROR) << "Size " << size << " is smaller than padding "
               << padding_bytes;
    return false;
  }

  if (!message->ParseFromArray(data, size - padding_bytes)) {
    LOG(ERROR) << "Failed to parse incoming metadata";
    return false;
  }
  return true;
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
