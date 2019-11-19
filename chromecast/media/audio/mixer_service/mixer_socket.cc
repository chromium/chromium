// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_socket.h"

#include <cstring>
#include <limits>
#include <utility>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/net/io_buffer_pool.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

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

}  // namespace

bool MixerSocket::Delegate::HandleMetadata(const Generic& message) {
  return true;
}

bool MixerSocket::Delegate::HandleAudioData(char* data,
                                            int size,
                                            int64_t timestamp) {
  return true;
}

bool MixerSocket::Delegate::HandleAudioBuffer(
    scoped_refptr<net::IOBuffer> buffer,
    char* data,
    int size,
    int64_t timestamp) {
  return HandleAudioData(data, size, timestamp);
}

// static
constexpr size_t MixerSocket::kAudioHeaderSize;
constexpr size_t MixerSocket::kAudioMessageHeaderSize;

MixerSocket::MixerSocket(std::unique_ptr<net::StreamSocket> socket)
    : socket_(std::make_unique<SmallMessageSocket>(this, std::move(socket))) {}

MixerSocket::MixerSocket() = default;

MixerSocket::~MixerSocket() {
  if (counterpart_task_runner_) {
    counterpart_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MixerSocket::OnEndOfStream, local_counterpart_));
  }
}

void MixerSocket::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  bool had_delegate = (delegate_ != nullptr);
  delegate_ = delegate;
  if (socket_ && !had_delegate) {
    socket_->ReceiveMessages();
  }
}

void MixerSocket::SetLocalCounterpart(
    base::WeakPtr<MixerSocket> local_counterpart,
    scoped_refptr<base::SequencedTaskRunner> counterpart_task_runner) {
  local_counterpart_ = std::move(local_counterpart);
  counterpart_task_runner_ = std::move(counterpart_task_runner);
}

base::WeakPtr<MixerSocket> MixerSocket::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MixerSocket::UseBufferPool(scoped_refptr<IOBufferPool> buffer_pool) {
  DCHECK(buffer_pool);
  DCHECK(buffer_pool->threadsafe());

  buffer_pool_ = std::move(buffer_pool);
  if (socket_) {
    socket_->UseBufferPool(buffer_pool_);
  }
}

// static
void MixerSocket::PrepareAudioBuffer(net::IOBuffer* audio_buffer,
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

void MixerSocket::SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                                  int filled_bytes,
                                  int64_t timestamp) {
  PrepareAudioBuffer(audio_buffer.get(), filled_bytes, timestamp);
  SendPreparedAudioBuffer(std::move(audio_buffer));
}

void MixerSocket::SendPreparedAudioBuffer(
    scoped_refptr<net::IOBuffer> audio_buffer) {
  uint16_t payload_size;
  base::ReadBigEndian(audio_buffer->data(), &payload_size);
  DCHECK_GE(payload_size, kAudioHeaderSize);
  SendBuffer(std::move(audio_buffer), sizeof(uint16_t) + payload_size);
}

void MixerSocket::SendProto(const google::protobuf::MessageLite& message) {
  int16_t type = static_cast<int16_t>(MessageType::kMetadata);
  int message_size = message.ByteSize();
  int32_t padding_bytes = (4 - (message_size % 4)) % 4;

  int total_size =
      sizeof(type) + sizeof(padding_bytes) + message_size + padding_bytes;

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

  base::WriteBigEndian(ptr, type);
  ptr += sizeof(type);
  base::WriteBigEndian(ptr, padding_bytes);
  ptr += sizeof(padding_bytes);
  message.SerializeToArray(ptr, message_size);
  ptr += message_size;
  memset(ptr, 0, padding_bytes);

  if (!buffer) {
    socket_->Send();
    return;
  }
  SendBuffer(std::move(buffer), sizeof(uint16_t) + total_size);
}

void MixerSocket::SendBuffer(scoped_refptr<net::IOBuffer> buffer,
                             int buffer_size) {
  if (counterpart_task_runner_) {
    counterpart_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&MixerSocket::OnMessageBuffer),
                       local_counterpart_, std::move(buffer), buffer_size));
    return;
  }
  DCHECK(socket_);
  if (!socket_->SendBuffer(buffer, buffer_size)) {
    write_queue_.push(std::move(buffer));
  }
}

void MixerSocket::OnSendUnblocked() {
  DCHECK(socket_);
  while (!write_queue_.empty()) {
    uint16_t message_size;
    base::ReadBigEndian(write_queue_.front()->data(), &message_size);
    if (!socket_->SendBuffer(write_queue_.front().get(),
                             sizeof(uint16_t) + message_size)) {
      return;
    }
    write_queue_.pop();
  }
}

void MixerSocket::ReceiveMoreMessages() {
  if (socket_) {
    socket_->ReceiveMessagesSynchronously();
  }
}

void MixerSocket::OnError(int error) {
  LOG(ERROR) << "Socket error from " << this << ": " << error;
  DCHECK(delegate_);
  delegate_->OnConnectionError();
}

void MixerSocket::OnEndOfStream() {
  DCHECK(delegate_);
  delegate_->OnConnectionError();
}

bool MixerSocket::OnMessage(char* data, int size) {
  int16_t type;
  if (size < static_cast<int>(sizeof(type))) {
    LOG(ERROR) << "Invalid message size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  memcpy(&type, data, sizeof(type));
  data += sizeof(type);
  size -= sizeof(type);

  switch (static_cast<MessageType>(type)) {
    case MessageType::kMetadata:
      return ParseMetadata(data, size);
    case MessageType::kAudio:
      return ParseAudio(data, size);
    default:
      return true;  // Ignore unhandled message types.
  }
}

bool MixerSocket::OnMessageBuffer(scoped_refptr<net::IOBuffer> buffer,
                                  int size) {
  if (size < static_cast<int>(sizeof(uint16_t) + sizeof(int16_t))) {
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
      return ParseMetadata(data, size);
    case MessageType::kAudio:
      return ParseAudioBuffer(std::move(buffer), data, size);
    default:
      return true;  // Ignore unhandled message types.
  }
}

bool MixerSocket::ParseMetadata(char* data, int size) {
  Generic message;
  if (!ReceiveProto(data, size, &message)) {
    LOG(INFO) << "Invalid metadata message from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  return delegate_->HandleMetadata(message);
}

bool MixerSocket::ParseAudio(char* data, int size) {
  int64_t timestamp;
  if (size < static_cast<int>(sizeof(timestamp))) {
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

bool MixerSocket::ParseAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                                   char* data,
                                   int size) {
  int64_t timestamp;
  if (size < static_cast<int>(sizeof(timestamp))) {
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

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
