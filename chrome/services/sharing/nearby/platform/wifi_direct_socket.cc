// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_socket.h"

#include <utility>

#include "base/check.h"
#include "base/threading/thread_restrictions.h"
#include "net/socket/stream_socket.h"

namespace nearby::chrome {

SocketInputStream::SocketInputStream() = default;

ExceptionOr<ByteArray> SocketInputStream::Read(std::int64_t size) {
  NOTIMPLEMENTED();
  return {Exception::kSuccess};
}

Exception SocketInputStream::Close() {
  return {Exception::kSuccess};
}

SocketOutputStream::SocketOutputStream() = default;

Exception SocketOutputStream::Write(const ByteArray& data) {
  NOTIMPLEMENTED();
  return {Exception::kSuccess};
}

Exception SocketOutputStream::Flush() {
  return {Exception::kSuccess};
}

Exception SocketOutputStream::Close() {
  return {Exception::kSuccess};
}

WifiDirectSocket::WifiDirectSocket(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<net::StreamSocket> stream_socket)
    : task_runner_(task_runner), stream_socket_(std::move(stream_socket)) {}

WifiDirectSocket::~WifiDirectSocket() {
  Close();
}

InputStream& WifiDirectSocket::GetInputStream() {
  return input_stream_;
}

OutputStream& WifiDirectSocket::GetOutputStream() {
  return output_stream_;
}

Exception WifiDirectSocket::Close() {
  if (!stream_socket_) {
    return {Exception::kFailed};
  }

  // Directly call `CloseSocket` if the current sequence is on the appropriate
  // task runner.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    CloseSocket(nullptr);
    return {Exception::kSuccess};
  }

  // Cleanup the socket on the IO thread.
  base::WaitableEvent waitable_event;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WifiDirectSocket::CloseSocket,
                                base::Unretained(this), &waitable_event));
  base::ScopedAllowBaseSyncPrimitives allow;
  waitable_event.Wait();

  return {Exception::kSuccess};
}

void WifiDirectSocket::CloseSocket(base::WaitableEvent* close_waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  stream_socket_.reset();
  if (close_waitable_event) {
    close_waitable_event->Signal();
  }
}

}  // namespace nearby::chrome
