// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_socket.h"

#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kSocketTrafficAnnotiation =
    net::DefineNetworkTrafficAnnotation("nearby_connections_wifi_direct", R"(
        semantics {
          sender: "Nearby Connections - WiFi Direct Medium"
          description:
            "The Nearby Connections WiFi Direct Medium transfers data over the "
            "network (eg. to send a file via Quick Share). This socket talks "
            "directly to another Nearby Connections client over an established "
            "WiFi Direct interface."
          trigger:
            "Nearby Connections successfully upgrades mediums to WiFi Direct."
          data:
            "After the WiFi Direct connection between devices is established, "
            "encrypted, and authenticated, feature-specific bytes are "
            "transferred. For example, Nearby Share might send/receive files "
            "and Phone Hub might receive message notification data from the "
            "phone."
          destination: OTHER
          destination_other:
            "A peer Nearby device that receives this data."
          internal {
            contacts {
              email: "jackshira@google.com"
            }
            contacts {
              email: "chromeos-cross-device-eng@google.com"
            }
          }
          user_data {
            type: ARBITRARY_DATA
          }
          last_reviewed: "2024-06-05"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is enabled for Nearby Connections clients that "
            "opt-in to WiFi Direct as an upgrade medium, which is handled on a "
            "client-by-client basis."
          policy_exception_justification:
            "The individual features that leverage Nearby Connections have "
            "their own policies associated with them."
        })");
}  // namespace

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
    UMA_HISTOGRAM_BOOLEAN("Nearby.Connections.WifiDirect.Socket.Read.Result",
                          false);
    return ExceptionOr<ByteArray>(exception.value());
  } else {
    UMA_HISTOGRAM_BOOLEAN("Nearby.Connections.WifiDirect.Socket.Read.Result",
                          true);
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

SocketOutputStream::SocketOutputStream(
    raw_ptr<net::StreamSocket> stream_socket,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : stream_socket_(stream_socket), task_runner_(task_runner) {}

SocketOutputStream::~SocketOutputStream() = default;

Exception SocketOutputStream::Write(const ByteArray& data) {
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data.string_data());
  auto response_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      std::move(buffer), data.size());

  base::WaitableEvent waitable_event;
  Exception output = {Exception::kFailed};
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SocketOutputStream::WriteToSocket, base::Unretained(this),
                     &response_buffer, &waitable_event, &output));
  waitable_event.Wait();

  UMA_HISTOGRAM_BOOLEAN("Nearby.Connections.WifiDirect.Socket.Write.Result",
                        output.Ok());
  return output;
}

Exception SocketOutputStream::Flush() {
  return {Exception::kSuccess};
}

Exception SocketOutputStream::Close() {
  stream_socket_ = nullptr;
  return {Exception::kSuccess};
}

void SocketOutputStream::WriteToSocket(
    scoped_refptr<net::DrainableIOBuffer>* buf,
    base::WaitableEvent* waitable_event,
    Exception* output) {
  if (!stream_socket_) {
    *output = Exception{Exception::kFailed};
    waitable_event->Signal();
    return;
  }

  auto result = stream_socket_->Write(
      buf->get(), buf->get()->BytesRemaining(),
      base::BindOnce(&SocketOutputStream::OnWrite, base::Unretained(this), buf,
                     waitable_event, output),
      kSocketTrafficAnnotiation);
  // If the `Write` call was unable to complete synchronously, a result value of
  // `ERR_IO_PENDING` is returned to indicate that the callback will be called
  // at some point in the future, when the write actually completes. If the call
  // is completed synchronously, the callback must be manually triggered.
  if (result != net::ERR_IO_PENDING) {
    OnWrite(buf, waitable_event, output, result);
  }
}

void SocketOutputStream::OnWrite(scoped_refptr<net::DrainableIOBuffer>* buf,
                                 base::WaitableEvent* waitable_event,
                                 Exception* output,
                                 int result) {
  if (result < 0) {
    *output = {Exception::kFailed};
    waitable_event->Signal();
    return;
  }

  buf->get()->DidConsume(result);
  if (buf->get()->BytesRemaining() > 0) {
    WriteToSocket(buf, waitable_event, output);
    return;
  }

  *output = {Exception::kSuccess};
  waitable_event->Signal();
}

WifiDirectSocket::WifiDirectSocket(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<net::StreamSocket> stream_socket)
    : task_runner_(task_runner),
      stream_socket_(std::move(stream_socket)),
      input_stream_(stream_socket_.get(), task_runner),
      output_stream_(stream_socket_.get(), task_runner) {}

WifiDirectSocket::WifiDirectSocket(
    mojo::PlatformHandle handle,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<net::StreamSocket> stream_socket)
    : handle_(std::move(handle)),
      task_runner_(task_runner),
      stream_socket_(std::move(stream_socket)),
      input_stream_(stream_socket_.get(), task_runner),
      output_stream_(stream_socket_.get(), task_runner) {}

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
  handle_.reset();

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
