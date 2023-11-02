// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/net/audio_socket.h"

#include <cstring>
#include <limits>
#include <utility>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/net/io_buffer_pool.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace chromecast {
namespace media {

namespace {

// First 2 bytes of each message indicate if it is metadata (protobuf) or audio.
enum class MessageType : int16_t {
  kMetadata,
  kAudio,
};

bool GetMetaDataPaddingBytes(const char* data,
                             size_t& size,
                             int32_t& padding_bytes) {
  if (size < sizeof(padding_bytes)) {
    LOG(ERROR) << "Invalid metadata message size " << size;
    return false;
  }

  base::ReadBigEndian(reinterpret_cast<const uint8_t*>(data), &padding_bytes);
  size -= sizeof(padding_bytes);

  if (padding_bytes < 0 || padding_bytes > 3) {
    LOG(ERROR) << "Invalid padding bytes count: " << padding_bytes;
    return false;
  }

  if (size < static_cast<size_t>(padding_bytes)) {
    LOG(ERROR) << "Size " << size << " is smaller than padding "
               << padding_bytes;
    return false;
  }

  return true;
}

}  // namespace

bool AudioSocket::Delegate::HandleAudioData(char* data,
                                            size_t size,
                                            int64_t timestamp) {
  return true;
}

bool AudioSocket::Delegate::HandleAudioBuffer(
    scoped_refptr<net::IOBuffer> buffer,
    char* data,
    size_t size,
    int64_t timestamp) {
  return HandleAudioData(data, size, timestamp);
}

// static
constexpr size_t AudioSocket::kAudioHeaderSize;
constexpr size_t AudioSocket::kAudioMessageHeaderSize;

AudioSocket::AudioSocket(std::unique_ptr<net::StreamSocket> socket)
    : socket_(std::make_unique<SmallMessageSocket>(this, std::move(socket))) {}

AudioSocket::AudioSocket() = default;

AudioSocket::~AudioSocket() {
  if (counterpart_task_runner_) {
    counterpart_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioSocket::OnEndOfStream, local_counterpart_));
  }
}

void AudioSocket::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  bool had_delegate = (delegate_ != nullptr);
  delegate_ = delegate;
  if (socket_ && !had_delegate) {
    socket_->ReceiveMessages();
  }
}

void AudioSocket::SetLocalCounterpart(
    base::WeakPtr<AudioSocket> local_counterpart,
    scoped_refptr<base::SequencedTaskRunner> counterpart_task_runner) {
  local_counterpart_ = std::move(local_counterpart);
  counterpart_task_runner_ = std::move(counterpart_task_runner);
}

base::WeakPtr<AudioSocket> AudioSocket::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AudioSocket::UseBufferPool(scoped_refptr<IOBufferPool> buffer_pool) {
  DCHECK(buffer_pool);
  DCHECK(buffer_pool->threadsafe());

  buffer_pool_ = std::move(buffer_pool);
  if (socket_) {
    socket_->UseBufferPool(buffer_pool_);
  }
}

// static
void AudioSocket::PrepareAudioBuffer(net::IOBuffer* audio_buffer,
                                     int filled_bytes,
                                     int64_t timestamp) {
  // Audio message format:
  //   uint16_t size (for SmallMessageSocket)
  //   uint16_t type (audio or metadata)
  //   uint64_t timestamp
  //   ... audio data ...
  int payload_size = kAudioHeaderSize + filled_bytes;
  uint16_t size = static_cast<uint16_t>(payload_size);
  int16_t type = static_cast<int16_t>(MessageType::kAudio);
  char* ptr = audio_buffer->data();

  base::WriteBigEndian(ptr, size);
  ptr += sizeof(size);
  memcpy(ptr, &type, sizeof(type));
  ptr += sizeof(type);
  memcpy(ptr, &timestamp, sizeof(timestamp));
  ptr += sizeof(timestamp);
  memset(ptr, 0, sizeof(int32_t));
}

bool AudioSocket::SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                                  int filled_bytes,
                                  int64_t timestamp) {
  PrepareAudioBuffer(audio_buffer.get(), filled_bytes, timestamp);
  return SendPreparedAudioBuffer(std::move(audio_buffer));
}

bool AudioSocket::SendPreparedAudioBuffer(
    scoped_refptr<net::IOBuffer> audio_buffer) {
  uint16_t payload_size;
  base::ReadBigEndian(reinterpret_cast<uint8_t*>(audio_buffer->data()),
                      &payload_size);
  DCHECK_GE(payload_size, kAudioHeaderSize);
  return SendBuffer(0, std::move(audio_buffer),
                    sizeof(uint16_t) + payload_size);
}

bool AudioSocket::SendProto(int type,
                            const google::protobuf::MessageLite& message) {
  int16_t packet_type = static_cast<int16_t>(MessageType::kMetadata);
  size_t message_size = message.ByteSizeLong();
  int32_t padding_bytes = (4 - (message_size % 4)) % 4;

  int total_size = sizeof(packet_type) + sizeof(padding_bytes) + message_size +
                   padding_bytes;

  scoped_refptr<net::IOBuffer> buffer;
  char* ptr = (socket_ ? static_cast<char*>(socket_->PrepareSend(total_size))
                       : nullptr);
  if (!ptr) {
    if (buffer_pool_ &&
        buffer_pool_->buffer_size() >= sizeof(uint16_t) + total_size) {
      buffer = buffer_pool_->GetBuffer();
    }
    if (!buffer) {
      buffer =
          base::MakeRefCounted<net::IOBuffer>(sizeof(uint16_t) + total_size);
    }
    ptr = buffer->data();
    base::WriteBigEndian(ptr, static_cast<uint16_t>(total_size));
    ptr += sizeof(uint16_t);
  }

  base::WriteBigEndian(ptr, packet_type);
  ptr += sizeof(packet_type);
  base::WriteBigEndian(ptr, padding_bytes);
  ptr += sizeof(padding_bytes);
  message.SerializeToArray(ptr, message_size);
  ptr += message_size;
  memset(ptr, 0, padding_bytes);

  if (!buffer) {
    socket_->Send();
    return true;
  }
  return SendBuffer(type, std::move(buffer), sizeof(uint16_t) + total_size);
}

bool AudioSocket::SendBuffer(int type,
                             scoped_refptr<net::IOBuffer> buffer,
                             size_t buffer_size) {
  if (counterpart_task_runner_) {
    counterpart_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&AudioSocket::OnMessageBuffer),
                       local_counterpart_, std::move(buffer), buffer_size));
    return true;
  }
  return SendBufferToSocket(type, std::move(buffer), buffer_size);
}

bool AudioSocket::SendBufferToSocket(int type,
                                     scoped_refptr<net::IOBuffer> buffer,
                                     size_t buffer_size) {
  DCHECK(socket_);
  if (!socket_->SendBuffer(buffer, buffer_size)) {
    if (type == 0) {
      return false;
    }
    pending_writes_.insert_or_assign(type, std::move(buffer));
  }
  return true;
}

void AudioSocket::OnSendUnblocked() {
  DCHECK(socket_);
  base::flat_map<int, scoped_refptr<net::IOBuffer>> pending;
  pending_writes_.swap(pending);
  for (auto& m : pending) {
    uint16_t message_size;
    base::ReadBigEndian(reinterpret_cast<uint8_t*>(m.second->data()),
                        &message_size);
    SendBufferToSocket(m.first, std::move(m.second),
                       sizeof(uint16_t) + message_size);
  }
}

void AudioSocket::ReceiveMoreMessages() {
  if (socket_) {
    socket_->ReceiveMessagesSynchronously();
  }
}

void AudioSocket::OnError(int error) {
  LOG(ERROR) << "Socket error from " << this << ": " << error;
  DCHECK(delegate_);
  delegate_->OnConnectionError();
}

void AudioSocket::OnEndOfStream() {
  DCHECK(delegate_);
  delegate_->OnConnectionError();
}

bool AudioSocket::OnMessage(char* data, size_t size) {
  int16_t packet_type;
  if (size < sizeof(packet_type)) {
    LOG(ERROR) << "Invalid message size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  memcpy(&packet_type, data, sizeof(packet_type));
  data += sizeof(packet_type);
  size -= sizeof(packet_type);

  switch (static_cast<MessageType>(packet_type)) {
    case MessageType::kMetadata:
      int32_t padding_bytes;
      if (!GetMetaDataPaddingBytes(data, size, padding_bytes)) {
        return false;
      }
      return ParseMetadata(data + sizeof(padding_bytes), size - padding_bytes);
    case MessageType::kAudio:
      return ParseAudio(data, size);
    default:
      return true;  // Ignore unhandled message types.
  }
}

bool AudioSocket::OnMessageBuffer(scoped_refptr<net::IOBuffer> buffer,
                                  size_t size) {
  if (size < sizeof(uint16_t) + sizeof(int16_t)) {
    LOG(ERROR) << "Invalid buffer size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  char* data = buffer->data() + sizeof(uint16_t);
  size -= sizeof(uint16_t);
  int16_t type;
  memcpy(&type, data, sizeof(type));
  data += sizeof(type);
  size -= sizeof(type);

  switch (static_cast<MessageType>(type)) {
    case MessageType::kMetadata:
      int32_t padding_bytes;
      if (!GetMetaDataPaddingBytes(data, size, padding_bytes)) {
        return false;
      }
      return ParseMetadata(data + sizeof(padding_bytes), size - padding_bytes);
    case MessageType::kAudio:
      return ParseAudioBuffer(std::move(buffer), data, size);
    default:
      return true;  // Ignore unhandled message types.
  }
}

bool AudioSocket::ParseAudio(char* data, size_t size) {
  int64_t timestamp;
  if (size < sizeof(timestamp)) {
    LOG(ERROR) << "Invalid audio packet size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  memcpy(&timestamp, data, sizeof(timestamp));
  data += sizeof(timestamp);
  size -= sizeof(timestamp);

  // Handle padding bytes.
  data += sizeof(int32_t);
  size -= sizeof(int32_t);

  return delegate_->HandleAudioData(data, size, timestamp);
}

bool AudioSocket::ParseAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                                   char* data,
                                   size_t size) {
  int64_t timestamp;
  if (size < sizeof(timestamp)) {
    LOG(ERROR) << "Invalid audio buffer size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  memcpy(&timestamp, data, sizeof(timestamp));
  data += sizeof(timestamp);
  size -= sizeof(timestamp);

  // Handle padding bytes.
  data += sizeof(int32_t);
  size -= sizeof(int32_t);

  return delegate_->HandleAudioBuffer(std::move(buffer), data, size, timestamp);
}

}  // namespace media
}  // namespace chromecast
