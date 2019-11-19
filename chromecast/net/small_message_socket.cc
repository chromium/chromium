// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/small_message_socket.h"

#include <stdint.h>
#include <string.h>

#include <limits>
#include <utility>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/net/io_buffer_pool.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

namespace chromecast {

namespace {

// Maximum number of times to read/write in a loop before reposting on the
// run loop (to allow other tasks to run).
const int kMaxIOLoop = 5;

const int kDefaultBufferSize = 2048;

}  // namespace

class SmallMessageSocket::BufferWrapper : public ::net::IOBuffer {
 public:
  void SetUnderlyingBuffer(scoped_refptr<IOBuffer> base, size_t size) {
    base_ = std::move(base);
    size_ = size;
    used_ = 0;
    data_ = base_->data();
  }

  scoped_refptr<IOBuffer> TakeUnderlyingBuffer() { return std::move(base_); }

  void ClearUnderlyingBuffer() {
    data_ = nullptr;
    base_.reset();
  }

  void DidConsume(size_t bytes) {
    used_ += bytes;
    data_ = base_->data() + used_;
  }

  char* StartOfBuffer() const {
    DCHECK(base_);
    return base_->data();
  }

  size_t size() const { return size_; }
  size_t used() const { return used_; }
  size_t remaining() const {
    DCHECK_GE(size_, used_);
    return size_ - used_;
  }

 private:
  ~BufferWrapper() override { data_ = nullptr; }

  scoped_refptr<IOBuffer> base_;
  size_t size_ = 0;
  size_t used_ = 0;
};

SmallMessageSocket::SmallMessageSocket(Delegate* delegate,
                                       std::unique_ptr<net::Socket> socket)
    : delegate_(delegate),
      socket_(std::move(socket)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      write_storage_(base::MakeRefCounted<net::GrowableIOBuffer>()),
      write_buffer_(base::MakeRefCounted<BufferWrapper>()),
      read_storage_(base::MakeRefCounted<net::GrowableIOBuffer>()),
      read_buffer_(base::MakeRefCounted<BufferWrapper>()),
      weak_factory_(this) {
  DCHECK(delegate_);
  write_storage_->SetCapacity(kDefaultBufferSize);
  read_storage_->SetCapacity(kDefaultBufferSize);
}

SmallMessageSocket::~SmallMessageSocket() = default;

void SmallMessageSocket::UseBufferPool(
    scoped_refptr<IOBufferPool> buffer_pool) {
  DCHECK(buffer_pool);
  if (buffer_pool_) {
    // Replace existing buffer pool. No need to copy data out of existing buffer
    // since it will remain valid until we are done using it.
    buffer_pool_ = std::move(buffer_pool);
    return;
  }

  buffer_pool_ = std::move(buffer_pool);
  if (!in_message_) {
    ActivateBufferPool(read_storage_->StartOfBuffer(), read_storage_->offset());
  }
}

void SmallMessageSocket::ActivateBufferPool(char* current_data,
                                            size_t current_size) {
  // Copy any already-read data into a new buffer for pool-based operation.
  DCHECK(buffer_pool_);
  DCHECK(!in_message_);

  scoped_refptr<::net::IOBuffer> new_buffer;
  size_t new_buffer_size;
  if (current_size <= buffer_pool_->buffer_size()) {
    new_buffer = buffer_pool_->GetBuffer();
    CHECK(new_buffer);
    new_buffer_size = buffer_pool_->buffer_size();
  } else {
    new_buffer = base::MakeRefCounted<::net::IOBuffer>(current_size * 2);
    new_buffer_size = current_size * 2;
  }
  memcpy(new_buffer->data(), current_data, current_size);

  read_buffer_->SetUnderlyingBuffer(std::move(new_buffer), new_buffer_size);
  read_buffer_->DidConsume(current_size);
}

void SmallMessageSocket::RemoveBufferPool() {
  if (!buffer_pool_) {
    return;
  }

  if (static_cast<size_t>(read_storage_->capacity()) < read_buffer_->used()) {
    read_storage_->SetCapacity(read_buffer_->used());
  }
  memcpy(read_storage_->StartOfBuffer(), read_buffer_->StartOfBuffer(),
         read_buffer_->used());
  read_storage_->set_offset(read_buffer_->used());

  buffer_pool_.reset();
}

void* SmallMessageSocket::PrepareSend(int message_size) {
  DCHECK_LE(message_size, std::numeric_limits<uint16_t>::max());
  if (write_buffer_->remaining()) {
    send_blocked_ = true;
    return nullptr;
  }

  write_storage_->set_offset(0);
  const int total_size = sizeof(uint16_t) + message_size;
  if (write_storage_->capacity() < total_size) {
    write_storage_->SetCapacity(total_size);
  }

  write_buffer_->SetUnderlyingBuffer(write_storage_, total_size);
  char* data = write_buffer_->data();
  base::WriteBigEndian(data, static_cast<uint16_t>(message_size));
  return data + sizeof(uint16_t);
}

bool SmallMessageSocket::SendBuffer(scoped_refptr<net::IOBuffer> data,
                                    int size) {
  if (write_buffer_->remaining()) {
    send_blocked_ = true;
    return false;
  }

  write_buffer_->SetUnderlyingBuffer(std::move(data), size);
  Send();
  return true;
}

void SmallMessageSocket::Send() {
  for (int i = 0; i < kMaxIOLoop; ++i) {
    int result =
        socket_->Write(write_buffer_.get(), write_buffer_->remaining(),
                       base::BindOnce(&SmallMessageSocket::OnWriteComplete,
                                      base::Unretained(this)),
                       MISSING_TRAFFIC_ANNOTATION);
    if (!HandleWriteResult(result)) {
      return;
    }
  }

  task_runner_->PostTask(FROM_HERE, base::BindOnce(&SmallMessageSocket::Send,
                                                   weak_factory_.GetWeakPtr()));
}

void SmallMessageSocket::OnWriteComplete(int result) {
  if (HandleWriteResult(result)) {
    Send();
  }
}

bool SmallMessageSocket::HandleWriteResult(int result) {
  if (result == net::ERR_IO_PENDING) {
    return false;
  }
  if (result <= 0) {
    // Post a task rather than just calling OnError(), to avoid calling
    // OnError()
    // synchronously.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&SmallMessageSocket::OnError,
                                          weak_factory_.GetWeakPtr(), result));
    return false;
  }

  write_buffer_->DidConsume(result);
  if (write_buffer_->remaining()) {
    return true;
  }

  write_buffer_->ClearUnderlyingBuffer();
  if (send_blocked_) {
    send_blocked_ = false;
    delegate_->OnSendUnblocked();
  }
  return false;
}

void SmallMessageSocket::OnError(int error) {
  delegate_->OnError(error);
}

void SmallMessageSocket::ReceiveMessages() {
  // Post a task rather than just calling Read(), to avoid calling delegate
  // methods from within this method.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SmallMessageSocket::ReceiveMessagesSynchronously,
                     weak_factory_.GetWeakPtr()));
}

void SmallMessageSocket::ReceiveMessagesSynchronously() {
  if ((buffer_pool_ && HandleCompletedMessageBuffers()) ||
      (!buffer_pool_ && HandleCompletedMessages())) {
    Read();
  }
}

void SmallMessageSocket::Read() {
  // Read in a loop for a few times while data is immediately available.
  // This improves average packet receive delay as compared to always posting a
  // new task for each call to Read().
  for (int i = 0; i < kMaxIOLoop; ++i) {
    net::IOBuffer* buffer;
    int size;
    if (buffer_pool_) {
      buffer = read_buffer_.get();
      size = read_buffer_->remaining();
    } else {
      buffer = read_storage_.get();
      size = read_storage_->RemainingCapacity();
    }
    int read_result =
        socket()->Read(buffer, size,
                       base::BindOnce(&SmallMessageSocket::OnReadComplete,
                                      base::Unretained(this)));

    if (!HandleReadResult(read_result)) {
      return;
    }
  }

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SmallMessageSocket::Read, weak_factory_.GetWeakPtr()));
}

void SmallMessageSocket::OnReadComplete(int result) {
  if (HandleReadResult(result)) {
    Read();
  }
}

bool SmallMessageSocket::HandleReadResult(int result) {
  if (result == net::ERR_IO_PENDING) {
    return false;
  }

  if (result == 0 || result == net::ERR_CONNECTION_CLOSED) {
    delegate_->OnEndOfStream();
    return false;
  }

  if (result < 0) {
    delegate_->OnError(result);
    return false;
  }

  if (buffer_pool_) {
    read_buffer_->DidConsume(result);
    return HandleCompletedMessageBuffers();
  } else {
    read_storage_->set_offset(read_storage_->offset() + result);
    return HandleCompletedMessages();
  }
}

bool SmallMessageSocket::HandleCompletedMessages() {
  DCHECK(!buffer_pool_);
  bool keep_reading = true;
  size_t bytes_read = read_storage_->offset();
  char* start_ptr = read_storage_->StartOfBuffer();
  while (bytes_read >= sizeof(uint16_t) && keep_reading) {
    uint16_t message_size;
    base::ReadBigEndian(start_ptr, &message_size);

    size_t required_size = sizeof(uint16_t) + message_size;
    if (static_cast<size_t>(read_storage_->capacity()) < required_size) {
      if (start_ptr != read_storage_->StartOfBuffer()) {
        memmove(read_storage_->StartOfBuffer(), start_ptr, bytes_read);
        read_storage_->set_offset(bytes_read);
      }
      read_storage_->SetCapacity(required_size);
      return true;
    }

    if (bytes_read < required_size) {
      break;  // Haven't received the full message yet.
    }

    // Take a weak pointer in case OnMessage() causes this to be deleted.
    auto self = weak_factory_.GetWeakPtr();
    in_message_ = true;
    keep_reading =
        delegate_->OnMessage(start_ptr + sizeof(uint16_t), message_size);
    if (!self) {
      return false;
    }
    in_message_ = false;

    start_ptr += required_size;
    bytes_read -= required_size;

    if (buffer_pool_) {
      // A buffer pool was added within OnMessage().
      ActivateBufferPool(start_ptr, bytes_read);
      return (keep_reading ? HandleCompletedMessageBuffers() : false);
    }
  }

  if (start_ptr != read_storage_->StartOfBuffer()) {
    memmove(read_storage_->StartOfBuffer(), start_ptr, bytes_read);
    read_storage_->set_offset(bytes_read);
  }

  return keep_reading;
}

bool SmallMessageSocket::HandleCompletedMessageBuffers() {
  DCHECK(buffer_pool_);
  size_t bytes_read;
  while ((bytes_read = read_buffer_->used()) >= sizeof(uint16_t)) {
    uint16_t message_size;
    base::ReadBigEndian(read_buffer_->StartOfBuffer(), &message_size);

    size_t required_size = sizeof(uint16_t) + message_size;
    if (read_buffer_->size() < required_size) {
      // Current buffer is not big enough.
      auto new_buffer = base::MakeRefCounted<::net::IOBuffer>(required_size);
      memcpy(new_buffer->data(), read_buffer_->StartOfBuffer(), bytes_read);
      read_buffer_->SetUnderlyingBuffer(std::move(new_buffer), required_size);
      read_buffer_->DidConsume(bytes_read);
      return true;
    }

    if (bytes_read < required_size) {
      break;  // Haven't received the full message yet.
    }

    auto old_buffer = read_buffer_->TakeUnderlyingBuffer();
    auto new_buffer = buffer_pool_->GetBuffer();
    CHECK(new_buffer);
    size_t new_buffer_size = buffer_pool_->buffer_size();
    size_t extra_size = bytes_read - required_size;
    if (extra_size > 0) {
      // Copy extra data to new buffer.
      if (extra_size > buffer_pool_->buffer_size()) {
        new_buffer = base::MakeRefCounted<::net::IOBuffer>(extra_size);
        new_buffer_size = extra_size;
      }
      memcpy(new_buffer->data(), old_buffer->data() + required_size,
             extra_size);
    }
    read_buffer_->SetUnderlyingBuffer(std::move(new_buffer), new_buffer_size);
    read_buffer_->DidConsume(extra_size);

    // Take a weak pointer in case OnMessageBuffer() causes this to be deleted.
    auto self = weak_factory_.GetWeakPtr();
    bool keep_reading =
        delegate_->OnMessageBuffer(std::move(old_buffer), required_size);
    if (!self || !keep_reading) {
      return false;
    }
    if (!buffer_pool_) {
      // The buffer pool was removed within OnMessageBuffer().
      return HandleCompletedMessages();
    }
  }

  return true;
}

bool SmallMessageSocket::Delegate::OnMessageBuffer(
    scoped_refptr<net::IOBuffer> buffer,
    int size) {
  return OnMessage(buffer->data() + sizeof(uint16_t), size - sizeof(uint16_t));
}

}  // namespace chromecast
