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

SmallMessageSocket::SmallMessageSocket(std::unique_ptr<net::Socket> socket)
    : socket_(std::move(socket)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {}

SmallMessageSocket::~SmallMessageSocket() = default;

void* SmallMessageSocket::PrepareSend(int message_size) {
  DCHECK_LE(message_size, std::numeric_limits<uint16_t>::max());
  if (write_buffer_) {
    send_blocked_ = true;
    return nullptr;
  }

  if (!write_storage_) {
    write_storage_ = base::MakeRefCounted<net::GrowableIOBuffer>();
  }

  write_storage_->set_offset(0);
  const int total_size = sizeof(uint16_t) + message_size;
  if (write_storage_->capacity() < total_size) {
    write_storage_->SetCapacity(total_size);
  }

  write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
      write_storage_.get(), total_size);
  char* data = write_buffer_->data();
  base::WriteBigEndian(data, static_cast<uint16_t>(message_size));
  return data + sizeof(uint16_t);
}

bool SmallMessageSocket::SendBuffer(net::IOBuffer* data, int size) {
  if (write_buffer_) {
    send_blocked_ = true;
    return false;
  }

  write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(data, size);
  Send();
  return true;
}

void SmallMessageSocket::Send() {
  for (int i = 0; i < kMaxIOLoop; ++i) {
    DCHECK(write_buffer_);
    // TODO(kmackay): Use base::BindOnce() once it is supported.
    int result =
        socket_->Write(write_buffer_.get(), write_buffer_->BytesRemaining(),
                       base::BindRepeating(&SmallMessageSocket::OnWriteComplete,
                                           base::Unretained(this)),
                       NO_TRAFFIC_ANNOTATION_YET);
    if (!HandleWriteResult(result)) {
      return;
    }
  }

  DCHECK(write_buffer_);
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
    PostError(result);
    return false;
  }

  write_buffer_->DidConsume(result);
  if (write_buffer_->BytesRemaining() != 0) {
    return true;
  }

  write_buffer_ = nullptr;
  if (send_blocked_) {
    send_blocked_ = false;
    OnSendUnblocked();
  }
  return false;
}

void SmallMessageSocket::PostError(int error) {
  // Post a task rather than just calling OnError(), to avoid calling OnError()
  // synchronously.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&SmallMessageSocket::OnError,
                                        weak_factory_.GetWeakPtr(), error));
}

void SmallMessageSocket::ReceiveMessages() {
  if (!read_buffer_) {
    read_buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
    read_buffer_->SetCapacity(kDefaultBufferSize);
  }
  // Post a task rather than just calling Read(), to avoid calling delegate
  // methods from within this method.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&SmallMessageSocket::StartReading,
                                        weak_factory_.GetWeakPtr()));
}

void SmallMessageSocket::StartReading() {
  if (HandleCompletedMessages()) {
    Read();
  }
}

void SmallMessageSocket::Read() {
  // Read in a loop for a few times while data is immediately available.
  // This improves average packet receive delay as compared to always posting a
  // new task for each call to Read().
  for (int i = 0; i < kMaxIOLoop; ++i) {
    // TODO(kmackay): Use base::BindOnce() once it is supported.
    int read_result =
        socket_->Read(read_buffer_.get(), read_buffer_->RemainingCapacity(),
                      base::BindRepeating(&SmallMessageSocket::OnReadComplete,
                                          base::Unretained(this)));

    if (!HandleReadResult(read_result)) {
      return;
    }
  }

  task_runner_->PostTask(FROM_HERE, base::BindOnce(&SmallMessageSocket::Read,
                                                   weak_factory_.GetWeakPtr()));
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
    OnEndOfStream();
    return false;
  }

  if (result < 0) {
    OnError(result);
    return false;
  }

  read_buffer_->set_offset(read_buffer_->offset() + result);
  return HandleCompletedMessages();
}

bool SmallMessageSocket::HandleCompletedMessages() {
  size_t total_size = read_buffer_->offset();
  char* start_ptr = read_buffer_->StartOfBuffer();
  bool keep_reading = true;

  while (total_size >= sizeof(uint16_t)) {
    uint16_t message_size;
    base::ReadBigEndian(start_ptr, &message_size);

    if (static_cast<size_t>(read_buffer_->capacity()) <
        sizeof(uint16_t) + message_size) {
      int position = start_ptr - read_buffer_->StartOfBuffer();
      read_buffer_->SetCapacity(sizeof(uint16_t) + message_size);
      start_ptr = read_buffer_->StartOfBuffer() + position;
    }

    if (total_size < sizeof(uint16_t) + message_size) {
      break;  // Haven't received the full message yet.
    }

    // Take a weak pointer in case OnMessage() causes this to be deleted.
    auto self = weak_factory_.GetWeakPtr();
    keep_reading = OnMessage(start_ptr + sizeof(uint16_t), message_size);
    if (!self) {
      return false;
    }

    total_size -= sizeof(uint16_t) + message_size;
    start_ptr += sizeof(uint16_t) + message_size;

    if (!keep_reading) {
      break;
    }
  }

  if (start_ptr != read_buffer_->StartOfBuffer()) {
    memmove(read_buffer_->StartOfBuffer(), start_ptr, total_size);
    read_buffer_->set_offset(total_size);
  }

  return keep_reading;
}

}  // namespace chromecast
