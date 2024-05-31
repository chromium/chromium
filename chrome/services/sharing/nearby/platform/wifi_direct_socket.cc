// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_socket.h"

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

WifiDirectSocket::WifiDirectSocket() = default;

WifiDirectSocket::~WifiDirectSocket() {}

InputStream& WifiDirectSocket::GetInputStream() {
  return input_stream_;
}

OutputStream& WifiDirectSocket::GetOutputStream() {
  return output_stream_;
}

Exception WifiDirectSocket::Close() {
  NOTIMPLEMENTED();
  return {Exception::kSuccess};
}

}  // namespace nearby::chrome
