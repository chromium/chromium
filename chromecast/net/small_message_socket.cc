// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/small_message_socket.h"

#include <stdint.h>
#include <string.h>

#include <limits>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/numerics/byte_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/net/io_buffer_pool.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

namespace chromecast {

namespace {

// Maximum number of times to read/write in a loop before reposting on the
// run loop (to allow other tasks to run).
const int kMaxIOLoop = 5;

const int kDefaultBufferSize = 2048;

constexpr size_t kMax2ByteSize = std::numeric_limits<uint16_t>::max();

}  // namespace

SmallMessageSocket::BufferWrapper::BufferWrapper() = default;
SmallMessageSocket::BufferWrapper::~BufferWrapper() {
  // The `data_` pointer in the base class is pointing into the buffer in the
  // `buffer_` field. Stop pointing into the field before its buffer is freed.
  data_ = nullptr;
}

void SmallMessageSocket::BufferWrapper::SetUnderlyingBuffer(
    scoped_refptr<IOBuffer> buffer,
    size_t capacity) {
  CHECK_LE(capacity, static_cast<size_t>(buffer->size()));

  buffer_ = std::move(buffer);
  used_ = 0;
  capacity_ = capacity;

  size_ = capacity_;
  data_ = buffer_->data();
}

scoped_refptr<net::IOBuffer>
SmallMessageSocket::BufferWrapper::TakeUnderlyingBuffer() {
  return std::move(buffer_);
}

void SmallMessageSocket::BufferWrapper::ClearUnderlyingBuffer() {
  data_ = nullptr;
  buffer_.reset();
}

void SmallMessageSocket::BufferWrapper::DidConsume(size_t bytes) {
  CHECK(buffer_);
  CHECK_LE(bytes, static_cast<size_t>(size_));

  size_ -= bytes;
  used_ += bytes;
  data_ += bytes;
  CHECK_EQ(data_, buffer_->data() + used_);
}

char* SmallMessageSocket::BufferWrapper::StartOfBuffer() const {
  CHECK(buffer_);
  return buffer_->data();
}

base::span<const uint8_t> SmallMessageSocket::BufferWrapper::used_span() const {
  CHECK(buffer_);
  return base::span(buffer_->bytes(), used_);
}

SmallMessageSocket::SmallMessageSocket(Delegate* delegate,
                                       std::unique_ptr<net::Socket> socket)
    : delegate_(delegate),
      socket_(std::move(socket)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
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
    ActivateBufferPool(read_storage_->span_before_offset());
  }
}

void SmallMessageSocket::ActivateBufferPool(
    base::span<const uint8_t> current_data) {
  // Copy any already-read data into a new buffer for pool-based operation.
  DCHECK(buffer_pool_);
  DCHECK(!in_message_);

  const size_t current_size = current_data.size();
  scoped_refptr<::net::IOBuffer> new_buffer;
  size_t new_buffer_size;
  if (current_size <= buffer_pool_->buffer_size()) {
    new_buffer = buffer_pool_->GetBuffer();
    CHECK(new_buffer);
    new_buffer_size = buffer_pool_->buffer_size();
  } else {
    new_buffer =
        base::MakeRefCounted<::net::IOBufferWithSize>(current_size * 2);
    new_buffer_size = current_size * 2;
  }
  base::as_writable_bytes(new_buffer->span()).copy_prefix_from(current_data);

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
  base::span<const uint8_t> used_span = read_buffer_->used_span();
  read_storage_->everything().copy_prefix_from(used_span);
  read_storage_->set_offset(used_span.size());

  buffer_pool_.reset();
}

void* SmallMessageSocket::PrepareSend(size_t message_size) {
  if (write_buffer_->size()) {
    send_blocked_ = true;
    return nullptr;
  }
  size_t bytes_for_size = SizeDataBytes(message_size);

  write_storage_->set_offset(0);
  const size_t total_size = bytes_for_size + message_size;
  // TODO(lethalantidote): Remove cast once capacity converted to size_t.
  if (static_cast<size_t>(write_storage_->capacity()) < total_size) {
    write_storage_->SetCapacity(total_size);
  }

  write_buffer_->SetUnderlyingBuffer(write_storage_, total_size);
  auto span = base::as_writable_bytes(write_buffer_->span());
  WriteSizeData(span, message_size);
  return span.subspan(bytes_for_size).data();
}

bool SmallMessageSocket::SendBuffer(scoped_refptr<net::IOBuffer> data,
                                    size_t size) {
  if (write_buffer_->size()) {
    send_blocked_ = true;
    return false;
  }

  write_buffer_->SetUnderlyingBuffer(std::move(data), size);
  Send();
  return true;
}

// static
size_t SmallMessageSocket::SizeDataBytes(size_t message_size) {
  return (message_size < kMax2ByteSize ? 2 : 6);
}

// static
size_t SmallMessageSocket::WriteSizeData(base::span<uint8_t> buf,
                                         size_t message_size) {
  base::SpanWriter writer(buf);
  if (message_size < kMax2ByteSize) {
    writer.WriteU16BigEndian(static_cast<uint16_t>(message_size));
  } else {
    writer.WriteU16BigEndian(base::checked_cast<uint16_t>(kMax2ByteSize));
    writer.WriteU32BigEndian(
        // NOTE: This may truncate the message_size.
        static_cast<uint32_t>(message_size));
  }
  return buf.size() - writer.remaining();
}

void SmallMessageSocket::Send() {
  for (int i = 0; i < kMaxIOLoop; ++i) {
    int result =
        socket_->Write(write_buffer_.get(), write_buffer_->size(),
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
  if (write_buffer_->size()) {
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
      size = read_buffer_->size();
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

// static
bool SmallMessageSocket::ReadSize(char* ptr,
                                  size_t bytes_read,
                                  size_t& data_offset,
                                  size_t& message_size) {
  if (bytes_read < sizeof(uint16_t)) {
    return false;
  }

  // TODO(crbug.com/40284755): This span is not safely constructed and is likely
  // incorrect for some callers. ReadSize() should receive a span instead of the
  // unbounded pointer `ptr`. We use up to bytes from the pointer below, so we
  // unsoundly claim that the span has 6 bytes here.
  auto span = UNSAFE_TODO(base::as_bytes(base::span(ptr, 6u)));

  uint16_t first_size = base::numerics::U16FromBigEndian(span.first<2u>());
  span = span.subspan(sizeof(uint16_t));
  data_offset = sizeof(uint16_t);
  if (first_size < kMax2ByteSize) {
    message_size = first_size;
  } else {
    if (bytes_read < sizeof(uint16_t) + sizeof(uint32_t)) {
      return false;
    }
    uint32_t real_size = base::numerics::U32FromBigEndian(span.first<4u>());
    span = span.subspan(sizeof(uint32_t));
    data_offset += sizeof(uint32_t);
    message_size = real_size;
  }
  return true;
}

bool SmallMessageSocket::HandleCompletedMessages() {
  DCHECK(!buffer_pool_);
  bool keep_reading = true;
  base::span<uint8_t> bytes_read = read_storage_->span_before_offset();
  while (keep_reading) {
    size_t data_offset;
    size_t message_size;
    if (!ReadSize(base::as_writable_chars(bytes_read).data(), bytes_read.size(),
                  data_offset, message_size)) {
      break;
    }
    size_t total_size = data_offset + message_size;

    if (static_cast<size_t>(read_storage_->capacity()) < total_size) {
      if (bytes_read != read_storage_->span_before_offset()) {
        read_storage_->everything().copy_prefix_from(bytes_read);
        read_storage_->set_offset(bytes_read.size());
      }
      read_storage_->SetCapacity(total_size);
      return true;
    }

    if (bytes_read.size() < total_size) {
      break;  // Haven't received the full message yet.
    }

    // Take a weak pointer in case OnMessage() causes this to be deleted.
    auto self = weak_factory_.GetWeakPtr();
    in_message_ = true;
    auto data =
        base::as_writable_chars(bytes_read.subspan(data_offset, message_size));
    keep_reading = delegate_->OnMessage(data.data(), data.size());
    if (!self) {
      return false;
    }
    in_message_ = false;

    bytes_read = bytes_read.subspan(total_size);

    if (buffer_pool_) {
      // A buffer pool was added within OnMessage().
      ActivateBufferPool(bytes_read);
      return (keep_reading ? HandleCompletedMessageBuffers() : false);
    }
  }

  if (bytes_read != read_storage_->span_before_offset()) {
    read_storage_->everything().copy_prefix_from(bytes_read);
    read_storage_->set_offset(bytes_read.size());
  }

  return keep_reading;
}

bool SmallMessageSocket::HandleCompletedMessageBuffers() {
  DCHECK(buffer_pool_);
  size_t bytes_read;
  while ((bytes_read = read_buffer_->used())) {
    size_t data_offset;
    size_t message_size;
    if (!ReadSize(read_buffer_->StartOfBuffer(), bytes_read, data_offset,
                  message_size)) {
      break;
    }
    size_t total_size = data_offset + message_size;

    if (read_buffer_->capacity() < total_size) {
      // Current buffer is not big enough.
      auto new_buffer =
          base::MakeRefCounted<::net::IOBufferWithSize>(total_size);
      memcpy(new_buffer->data(), read_buffer_->StartOfBuffer(), bytes_read);
      read_buffer_->SetUnderlyingBuffer(std::move(new_buffer), total_size);
      read_buffer_->DidConsume(bytes_read);
      return true;
    }

    if (bytes_read < total_size) {
      break;  // Haven't received the full message yet.
    }

    auto old_buffer = read_buffer_->TakeUnderlyingBuffer();
    auto new_buffer = buffer_pool_->GetBuffer();
    CHECK(new_buffer);
    size_t new_buffer_size = buffer_pool_->buffer_size();
    size_t extra_size = bytes_read - total_size;
    if (extra_size > 0) {
      // Copy extra data to new buffer.
      if (extra_size > buffer_pool_->buffer_size()) {
        new_buffer = base::MakeRefCounted<::net::IOBufferWithSize>(extra_size);
        new_buffer_size = extra_size;
      }
      memcpy(new_buffer->data(), old_buffer->data() + total_size, extra_size);
    }
    read_buffer_->SetUnderlyingBuffer(std::move(new_buffer), new_buffer_size);
    read_buffer_->DidConsume(extra_size);

    // Take a weak pointer in case OnMessageBuffer() causes this to be deleted.
    auto self = weak_factory_.GetWeakPtr();
    bool keep_reading =
        delegate_->OnMessageBuffer(std::move(old_buffer), total_size);
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
    size_t size) {
  size_t offset = SizeDataBytes(size);
  return OnMessage(buffer->data() + offset, size - offset);
}

}  // namespace chromecast
