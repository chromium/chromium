// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SOCKET_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/nearby/src/internal/platform/byte_array.h"
#include "third_party/nearby/src/internal/platform/exception.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_direct.h"
#include "third_party/nearby/src/internal/platform/input_stream.h"
#include "third_party/nearby/src/internal/platform/output_stream.h"

namespace net {
class StreamSocket;
class IOBufferWithSize;
class DrainableIOBuffer;
}

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace nearby::chrome {

class SocketInputStream : public InputStream {
 public:
  SocketInputStream(raw_ptr<net::StreamSocket> stream_socket,
                    scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~SocketInputStream() override;
  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  void ReadFromSocket(scoped_refptr<net::IOBufferWithSize>* buffer,
                      std::int64_t buffer_len,
                      int* bytes_read,
                      std::optional<Exception>* exception,
                      base::WaitableEvent* waitable_event);
  void OnRead(int* bytes_read,
              std::optional<Exception>* exception,
              base::WaitableEvent* waitable_event,
              int result);

  raw_ptr<net::StreamSocket> stream_socket_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

class SocketOutputStream : public OutputStream {
 public:
  SocketOutputStream(raw_ptr<net::StreamSocket> stream_socket,
                     scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~SocketOutputStream() override;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  void WriteToSocket(scoped_refptr<net::DrainableIOBuffer>* buf,
                     base::WaitableEvent* waitable_event,
                     Exception* output);
  void OnWrite(scoped_refptr<net::DrainableIOBuffer>* buf,
               base::WaitableEvent* waitable_event,
               Exception* output,
               int result);

  raw_ptr<net::StreamSocket> stream_socket_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// This class takes ownership of a socket that must be operated on the provided
// `task_runner`. Specifically, socket operations (including destruction) on
// ChromeOS need to occur on an IO thread on the same sequence the socket was
// created on. There are no guarantees that Nearby Connections will call these
// operations on the appropriate sequence, so this implementation needs to
// ensure the required sequence is always used.
class WifiDirectSocket : public api::WifiDirectSocket {
 public:
  WifiDirectSocket(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   std::unique_ptr<net::StreamSocket> stream_socket);
  WifiDirectSocket(mojo::PlatformHandle handle,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   std::unique_ptr<net::StreamSocket> stream_socket);
  ~WifiDirectSocket() override;

  // api::WifiDirectSocket
  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;

 private:
  // Called by `Close` to ensure the socket is closed on the sequence it was
  // originally created on.
  void CloseSocket(base::WaitableEvent* close_waitable_event);

  mojo::PlatformHandle handle_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<net::StreamSocket> stream_socket_;
  SocketInputStream input_stream_;
  SocketOutputStream output_stream_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SOCKET_H_
