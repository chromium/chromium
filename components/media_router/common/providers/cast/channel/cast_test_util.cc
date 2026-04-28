// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_test_util.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "net/base/ip_address.h"

namespace cast_channel {

MockCastTransport::MockCastTransport() = default;
MockCastTransport::~MockCastTransport() = default;

CastTransport::Delegate* MockCastTransport::current_delegate() const {
  CHECK(delegate_);
  return delegate_.get();
}

void MockCastTransport::SetReadDelegate(
    std::unique_ptr<CastTransport::Delegate> delegate) {
  delegate_ = std::move(delegate);
}

MockCastTransportDelegate::MockCastTransportDelegate() = default;
MockCastTransportDelegate::~MockCastTransportDelegate() = default;

MockCastSocketObserver::MockCastSocketObserver() = default;
MockCastSocketObserver::~MockCastSocketObserver() = default;

MockCastSocketService::MockCastSocketService(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  SetTaskRunnerForTest(task_runner);
}
MockCastSocketService::~MockCastSocketService() = default;

MockCastSocket::MockCastSocket()
    : channel_id_(0),
      error_state_(ChannelError::NONE),
      keep_alive_(false),
      audio_only_(false),
      mock_transport_(new MockCastTransport()) {}
MockCastSocket::~MockCastSocket() = default;

net::IPEndPoint CreateIPEndPointForTest() {
  return net::IPEndPoint(net::IPAddress(192, 168, 1, 1), 8009);
}

MockCastMessageHandler::MockCastMessageHandler(
    MockCastSocketService* socket_service)
    : CastMessageHandler(socket_service,
                         /* parse_json */ base::DoNothing(),
                         "userAgent",
                         "1.2.3.4",
                         "en-US") {}

MockCastMessageHandler::~MockCastMessageHandler() = default;

}  // namespace cast_channel
