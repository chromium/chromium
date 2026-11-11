// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_PROTO_STREAM_MOCK_MESSAGE_STREAM_CLIENT_H_
#define COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_PROTO_STREAM_MOCK_MESSAGE_STREAM_CLIENT_H_

#include <string>

#include "components/stream_transport/public/proto_stream/message_stream_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace stream_transport {

class MockMessageStreamClient : public MessageStreamClient {
 public:
  MockMessageStreamClient();
  ~MockMessageStreamClient() override;

  MOCK_METHOD(void, Connect, (), (override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(bool, IsConnected, (), (const, override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
};

class MockMessageStreamObserver : public MessageStreamClient::Observer {
 public:
  MockMessageStreamObserver();
  ~MockMessageStreamObserver() override;

  MOCK_METHOD(void, OnStreamMessage, (const std::string&), (override));
  MOCK_METHOD(void, OnStreamStatus, (const std::string&), (override));
  MOCK_METHOD(void, OnStreamConnectionStateChange, (bool), (override));
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_PROTO_STREAM_MOCK_MESSAGE_STREAM_CLIENT_H_
