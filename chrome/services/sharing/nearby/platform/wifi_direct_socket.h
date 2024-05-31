// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SOCKET_H_

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"
#include "third_party/nearby/src/internal/platform/byte_array.h"
#include "third_party/nearby/src/internal/platform/exception.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_direct.h"
#include "third_party/nearby/src/internal/platform/input_stream.h"
#include "third_party/nearby/src/internal/platform/output_stream.h"

namespace nearby::chrome {

class SocketInputStream : public InputStream {
 public:
  SocketInputStream();
  ~SocketInputStream() override = default;
  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;
};

class SocketOutputStream : public OutputStream {
 public:
  SocketOutputStream();
  ~SocketOutputStream() override = default;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;
};

class WifiDirectSocket : public api::WifiDirectSocket {
 public:
  WifiDirectSocket();
  ~WifiDirectSocket() override;

  // api::WifiDirectSocket
  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;

 private:
  SocketInputStream input_stream_;
  SocketOutputStream output_stream_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SOCKET_H_
