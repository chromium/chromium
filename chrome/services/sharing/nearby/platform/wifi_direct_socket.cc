// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_socket.h"

#include <utility>

#include "base/check.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

namespace nearby::chrome {

SocketInputStream::SocketInputStream(
    raw_ptr<net::StreamSocket> stream_socket,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : stream_socket_(stream_socket), task_runner_(task_runner) {}

SocketInputStream::~SocketInputStream() = default;

ExceptionOr<ByteArray> SocketInputStream::Read(std::int64_t size) {
  base::WaitableEvent waitable_event;
  auto response_buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);
  int bytes_read = 0;
  std::optional<Exception> exception = std::nullopt;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SocketInputStream::ReadFromSocket,
                                base::Unretained(this), &response_buffer, size,
                                &bytes_read, &exception, &waitable_event));
  waitable_event.Wait();

  if (exception) {
    return ExceptionOr<ByteArray>(exception.value());
  } else {
    return ExceptionOr<ByteArray>(
        ByteArray(response_buffer->data(), bytes_read));
  }
}

Exception SocketInputStream::Close() {
  // This input stream does not own the socket, so it should not be responsible
  // for closing it.
  stream_socket_ = nullptr;
  return {Exception::kSuccess};
}

void SocketInputStream::ReadFromSocket(
    scoped_refptr<net::IOBufferWithSize>* buffer,
    std::int64_t buffer_len,
    int* bytes_read,
    std::optional<Exception>* exception,
    base::WaitableEvent* waitable_event) {
  if (!stream_socket_) {
    *exception = Exception{Exception::kFailed};
    *bytes_read = 0;
    waitable_event->Signal();
    return;
  }

  auto result = stream_socket_->Read(
      buffer->get(), buffer_len,
      base::BindOnce(&SocketInputStream::OnRead, base::Unretained(this),
                     bytes_read, exception, waitable_event));
  // If the `Read` call was unable to complete synchronously, a result value of
  // `ERR_IO_PENDING` is returned to indicate that the callback will be called
  // at some point in the future, when the read actually completes. If the call
  // is completed synchronously, the callback must be manually triggered.
  if (result != net::ERR_IO_PENDING) {
    OnRead(bytes_read, exception, waitable_event, result);
  }
}

void SocketInputStream::OnRead(int* bytes_read,
                               std::optional<Exception>* exception,
                               base::WaitableEvent* waitable_event,
                               int result) {
  if (result < 0) {
    *exception = Exception{Exception::kFailed};
    *bytes_read = 0;
  } else {
    *exception = std::nullopt;
    *bytes_read = result;
  }
  waitable_event->Signal();
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
    : task_runner_(task_runner),
      stream_socket_(std::move(stream_socket)),
      input_stream_(stream_socket_.get(), task_runner_) {}

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

  // Propagate the close signal to the streams so they clean up their pointers
  // to the underlying `net::StreamSocket`.
  input_stream_.Close();
  output_stream_.Close();

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
