// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_TEST_UTIL_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_TEST_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/media_router/common/providers/cast/channel/cast_transport.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {

class MockCastTransport : public CastTransport {
 public:
  MockCastTransport();

  MockCastTransport(const MockCastTransport&) = delete;
  MockCastTransport& operator=(const MockCastTransport&) = delete;

  ~MockCastTransport() override;

  void SetReadDelegate(
      std::unique_ptr<CastTransport::Delegate> delegate) override;

  void SendMessage(const CastMessage& message,
                   net::CompletionOnceCallback callback) override {
    // GMock does not handle move-only types, we need to rely on a mock method
    // that takes a repeating callback, which will work well with GMock actions.
    SendMessage_(message, callback);
  }

  MOCK_METHOD(void,
              SendMessage_,
              (const CastMessage& message,
               net::CompletionOnceCallback& callback),
              ());

  MOCK_METHOD(void, Start, (), (override));

  // Gets the read delegate that is currently active for this transport.
  CastTransport::Delegate* current_delegate() const;

 private:
  std::unique_ptr<CastTransport::Delegate> delegate_;
};

class MockCastTransportDelegate : public CastTransport::Delegate {
 public:
  MockCastTransportDelegate();

  MockCastTransportDelegate(const MockCastTransportDelegate&) = delete;
  MockCastTransportDelegate& operator=(const MockCastTransportDelegate&) =
      delete;

  ~MockCastTransportDelegate() override;

  MOCK_METHOD(void, OnError, (ChannelError error), (override));
  MOCK_METHOD(void, OnMessage, (const CastMessage& message), (override));
  MOCK_METHOD(void, Start, (), (override));
};

class MockCastSocketObserver : public CastSocket::Observer {
 public:
  MockCastSocketObserver();
  ~MockCastSocketObserver() override;

  MOCK_METHOD(void,
              OnError,
              (const CastSocket& socket, ChannelError error),
              (override));
  MOCK_METHOD(void,
              OnMessage,
              (const CastSocket& socket, const CastMessage& message),
              (override));
  MOCK_METHOD(void,
              OnReadyStateChanged,
              (const CastSocket& socket),
              (override));
};

class MockCastSocketService : public CastSocketServiceImpl {
 public:
  explicit MockCastSocketService(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~MockCastSocketService() override;

  void OpenSocket(network::NetworkContextGetter network_context_getter,
                  const CastSocketOpenParams& open_params,
                  CastSocket::OnOpenCallback open_cb) override {
    OpenSocket_(open_params.ip_endpoint, open_cb);
  }

  MOCK_METHOD(void,
              OpenSocket_,
              (const net::IPEndPoint& ip_endpoint,
               CastSocket::OnOpenCallback& open_cb),
              ());
  MOCK_METHOD(CastSocket*, GetSocket, (int channel_id), (const, override));
  MOCK_METHOD(CastSocket*,
              GetSocket,
              (const net::IPEndPoint& ip_endpoint),
              (const, override));
  MOCK_METHOD(std::unique_ptr<CastSocket>,
              RemoveSocket,
              (int channel_id),
              (override));
  MOCK_METHOD(void, CloseSocket, (int channel_id), (override));
};

class MockCastSocket : public CastSocket {
 public:
  using MockOnOpenCallback = base::RepeatingCallback<void(CastSocket* socket)>;

  MockCastSocket();

  MockCastSocket(const MockCastSocket&) = delete;
  MockCastSocket& operator=(const MockCastSocket&) = delete;

  ~MockCastSocket() override;

  void Connect(CastSocket::OnOpenCallback callback) override {
    Connect_(callback);
  }

  void Close(net::CompletionOnceCallback callback) override {
    // GMock does not handle move-only types, we need to rely on a mock method
    // that takes a repeating callback, which will work well with GMock actions.
    Close_(callback);
  }

  MOCK_METHOD(void, Connect_, (CastSocket::OnOpenCallback & callback), ());
  MOCK_METHOD(void, Close_, (net::CompletionOnceCallback & callback), ());
  MOCK_METHOD(ReadyState, ready_state, (), (const, override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));

  const net::IPEndPoint& ip_endpoint() const override { return ip_endpoint_; }
  void SetIPEndpoint(const net::IPEndPoint& ip_endpoint) {
    ip_endpoint_ = ip_endpoint;
  }

  int id() const override { return channel_id_; }
  void set_id(int id) override { channel_id_ = id; }

  ChannelError error_state() const override { return error_state_; }
  void SetErrorState(ChannelError error_state) override {
    error_state_ = error_state;
  }

  CastChannelFlags flags() const override { return flags_; }
  void SetFlags(CastChannelFlags flags) { flags_ = flags; }

  bool keep_alive() const override { return keep_alive_; }
  void SetKeepAlive(bool keep_alive) { keep_alive_ = keep_alive; }

  bool audio_only() const override { return audio_only_; }
  void SetAudioOnly(bool audio_only) { audio_only_ = audio_only; }

  CastTransport* transport() const override { return mock_transport_.get(); }
  MockCastTransport* mock_transport() const { return mock_transport_.get(); }

 private:
  net::IPEndPoint ip_endpoint_;
  int channel_id_;
  ChannelError error_state_;
  CastChannelFlags flags_{kCastChannelFlagsNone};
  bool keep_alive_;
  bool audio_only_;

  std::unique_ptr<MockCastTransport> mock_transport_;
  std::unique_ptr<Observer> observer_;
};

class MockCastMessageHandler : public CastMessageHandler {
 public:
  explicit MockCastMessageHandler(MockCastSocketService* socket_service);

  MockCastMessageHandler(const MockCastMessageHandler&) = delete;
  MockCastMessageHandler& operator=(const MockCastMessageHandler&) = delete;

  ~MockCastMessageHandler() override;

  MOCK_METHOD(void,
              EnsureConnection,
              (int,
               const std::string&,
               const std::string&,
               VirtualConnectionType connection_type),
              (override));
  MOCK_METHOD(void,
              CloseConnection,
              (int, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              RemoveConnection,
              (int, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              RequestAppAvailability,
              (CastSocket * socket,
               const std::string& app_id,
               GetAppAvailabilityCallback callback),
              (override));
  MOCK_METHOD(void, RequestReceiverStatus, (int channel_id), (override));
  MOCK_METHOD(void,
              LaunchSession,
              (int,
               const std::string&,
               base::TimeDelta,
               const std::vector<std::string>&,
               const std::optional<base::Value>&,
               LaunchSessionCallback callback),
              (override));
  MOCK_METHOD(void,
              StopSession,
              (int channel_id,
               const std::string& session_id,
               const std::optional<std::string>& client_id,
               ResultCallback callback),
              (override));
  MOCK_METHOD(Result,
              SendAppMessage,
              (int channel_id, const CastMessage& message),
              (override));
  MOCK_METHOD(Result,
              SendCastMessage,
              (int channel_id, const CastMessage& message),
              (override));
  MOCK_METHOD(std::optional<int>,
              SendMediaRequest,
              (int channel_id,
               const base::Value::Dict& body,
               const std::string& source_id,
               const std::string& destination_id),
              (override));
  MOCK_METHOD(void,
              SendSetVolumeRequest,
              (int channel_id,
               const base::Value::Dict& body,
               const std::string& source_id,
               ResultCallback callback),
              (override));
};

// Creates the IPEndpoint 192.168.1.1.
net::IPEndPoint CreateIPEndPointForTest();

// Checks if two proto messages are the same.
// From
// third_party/cacheinvalidation/overrides/google/cacheinvalidation/deps/gmock.h
// TODO(kmarshall): promote to a shared testing library.
MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

ACTION_TEMPLATE(PostCompletionCallbackTask,
                HAS_1_TEMPLATE_PARAMS(int, cb_idx),
                AND_1_VALUE_PARAMS(rv)) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(testing::get<cb_idx>(args)), rv));
}

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_TEST_UTIL_H_
