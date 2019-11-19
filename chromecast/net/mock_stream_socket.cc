// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/mock_stream_socket.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

namespace chromecast {

MockStreamSocket::MockStreamSocket() {
  // Set default return values.
  ON_CALL(*this, Read(_, _, _)).WillByDefault(Return(net::ERR_IO_PENDING));
  ON_CALL(*this, Write(_, _, _, _))
      .WillByDefault(Invoke(
          [](net::IOBuffer* buf, int buf_len,
             net::CompletionOnceCallback callback,
             const net::NetworkTrafficAnnotationTag& traffic_annotation) {
            return buf_len;
          }));
  ON_CALL(*this, NetLog()).WillByDefault(ReturnRef(net_log_));
  ON_CALL(*this, GetNegotiatedProtocol())
      .WillByDefault(Return(net::NextProto()));
}

MockStreamSocket::~MockStreamSocket() {}

}  // namespace chromecast
