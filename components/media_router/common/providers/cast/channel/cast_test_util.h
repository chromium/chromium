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

  MOCK_METHOD2(SendMessage_,
               void(const CastMessage& message,
                    net::CompletionOnceCallback& callback));

  MOCK_METHOD0(Start, void(void));

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

  MOCK_METHOD1(OnError, void(ChannelError error));
  MOCK_METHOD1(OnMessage, void(const CastMessage& message));
  MOCK_METHOD0(Start, void(void));
};

class MockCastSocketObserver : public CastSocket::Observer {
 public:
  MockCastSocketObserver();
  ~MockCastSocketObserver() override;

  MOCK_METHOD2(OnError, void(const CastSocket& socket, ChannelError error));
  MOCK_METHOD2(OnMessage,
               void(const CastSocket& socket, const CastMessage& message));
};

class MockCastSocketService : public CastSocketServiceImpl {
 public:
  explicit MockCastSocketService(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~MockCastSocketService() override;

  void OpenSocket(NetworkContextGetter network_context_getter,
                  const CastSocketOpenParams& open_params,
                  CastSocket::OnOpenCallback open_cb) override {
    OpenSocket_(open_params.ip_endpoint, open_cb);
  }

  MOCK_METHOD2(OpenSocket_,
               void(const net::IPEndPoint& ip_endpoint,
                    CastSocket::OnOpenCallback& open_cb));
  MOCK_CONST_METHOD1(GetSocket, CastSocket*(int channel_id));
  MOCK_METHOD(CastSocket*,
              GetSocket,
              (const net::IPEndPoint& ip_endpoint),
              (override, const));
  MOCK_METHOD(std::unique_ptr<CastSocket>, RemoveSocket, (int channel_id), ());
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

  MOCK_METHOD1(Connect_, void(CastSocket::OnOpenCallback& callback));
  MOCK_METHOD1(Close_, void(net::CompletionOnceCallback& callback));
  MOCK_CONST_METHOD0(ready_state, ReadyState());
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));

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

  CastChannelFlags flags() const override {
    return static_cast<CastChannelFlags>(CastChannelFlag::kFlagsNone);
  }

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

  MOCK_METHOD4(EnsureConnection,
               void(int,
                    const std::string&,
                    const std::string&,
                    VirtualConnectionType connection_type));
  MOCK_METHOD3(CloseConnection,
               void(int, const std::string&, const std::string&));
  MOCK_METHOD3(RequestAppAvailability,
               void(CastSocket* socket,
                    const std::string& app_id,
                    GetAppAvailabilityCallback callback));
  MOCK_METHOD1(RequestReceiverStatus, void(int channel_id));
  MOCK_METHOD3(SendBroadcastMessage,
               Result(int,
                      const std::vector<std::string>&,
                      const BroadcastRequest&));
  MOCK_METHOD6(LaunchSession,
               void(int,
                    const std::string&,
                    base::TimeDelta,
                    const std::vector<std::string>&,
                    const absl::optional<base::Value>&,
                    LaunchSessionCallback callback));
  MOCK_METHOD4(StopSession,
               void(int channel_id,
                    const std::string& session_id,
                    const absl::optional<std::string>& client_id,
                    ResultCallback callback));
  MOCK_METHOD2(SendAppMessage,
               Result(int channel_id, const CastMessage& message));
  MOCK_METHOD2(SendCastMessage,
               Result(int channel_id, const CastMessage& message));
  MOCK_METHOD4(SendMediaRequest,
               absl::optional<int>(int channel_id,
                                   const base::Value::Dict& body,
                                   const std::string& source_id,
                                   const std::string& destination_id));
  MOCK_METHOD4(SendSetVolumeRequest,
               void(int channel_id,
                    const base::Value::Dict& body,
                    const std::string& source_id,
                    ResultCallback callback));
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
