// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_TEST_UTIL_H_
#define COMPONENTS_CAST_CHANNEL_CAST_TEST_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_transport.h"
#include "components/cast_channel/proto/cast_channel.pb.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cast_channel {

class MockCastTransport : public CastTransport {
 public:
  MockCastTransport();
  ~MockCastTransport() override;

  void SetReadDelegate(
      std::unique_ptr<CastTransport::Delegate> delegate) override;

  void SendMessage(const CastMessage& message,
                   net::CompletionOnceCallback callback) override {
    // GMock does not handle move-only types, we need to rely on a mock method
    // that takes a repeating callback, which will work well with GMock actions.
    SendMessage(message, base::AdaptCallbackForRepeating(std::move(callback)));
  }

  MOCK_METHOD2(SendMessage,
               void(const CastMessage& message,
                    const net::CompletionRepeatingCallback& callback));

  MOCK_METHOD0(Start, void(void));

  // Gets the read delegate that is currently active for this transport.
  CastTransport::Delegate* current_delegate() const;

 private:
  std::unique_ptr<CastTransport::Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockCastTransport);
};

class MockCastTransportDelegate : public CastTransport::Delegate {
 public:
  MockCastTransportDelegate();
  ~MockCastTransportDelegate() override;

  MOCK_METHOD1(OnError, void(ChannelError error));
  MOCK_METHOD1(OnMessage, void(const CastMessage& message));
  MOCK_METHOD0(Start, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastTransportDelegate);
};

class MockCastSocketObserver : public CastSocket::Observer {
 public:
  MockCastSocketObserver();
  ~MockCastSocketObserver() override;

  MOCK_METHOD2(OnError, void(const CastSocket& socket, ChannelError error));
  MOCK_METHOD2(OnMessage,
               void(const CastSocket& socket, const CastMessage& message));
};

class MockCastSocketService : public CastSocketService {
 public:
  explicit MockCastSocketService(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~MockCastSocketService() override;

  void OpenSocket(NetworkContextGetter network_context_getter,
                  const CastSocketOpenParams& open_params,
                  CastSocket::OnOpenCallback open_cb) override {
    // Unit test should not call |open_cb| more than once. Just use
    // base::AdaptCallbackForRepeating to pass |open_cb| to a mock method.
    OpenSocketInternal(open_params.ip_endpoint,
                       base::AdaptCallbackForRepeating(std::move(open_cb)));
  }

  MOCK_METHOD2(OpenSocketInternal,
               void(const net::IPEndPoint& ip_endpoint,
                    const base::Callback<void(CastSocket*)>& open_cb));
  MOCK_CONST_METHOD1(GetSocket, CastSocket*(int channel_id));
};

class MockCastSocket : public CastSocket {
 public:
  using MockOnOpenCallback = base::Callback<void(CastSocket* socket)>;

  MockCastSocket();
  ~MockCastSocket() override;

  void Connect(CastSocket::OnOpenCallback callback) override {
    // Unit test should not call |open_cb| more than once. Just use
    // base::AdaptCallbackForRepeating to pass |open_cb| to a mock method.
    ConnectInternal(base::AdaptCallbackForRepeating(std::move(callback)));
  }

  void Close(net::CompletionOnceCallback callback) override {
    // GMock does not handle move-only types, we need to rely on a mock method
    // that takes a repeating callback, which will work well with GMock actions.
    Close(base::AdaptCallbackForRepeating(std::move(callback)));
  }

  MOCK_METHOD1(ConnectInternal, void(const MockOnOpenCallback& callback));
  MOCK_METHOD1(Close, void(const net::CompletionRepeatingCallback& callback));
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

  DISALLOW_COPY_AND_ASSIGN(MockCastSocket);
};

class MockCastMessageHandler : public CastMessageHandler {
 public:
  explicit MockCastMessageHandler(MockCastSocketService* socket_service);
  ~MockCastMessageHandler() override;

  MOCK_METHOD3(EnsureConnection,
               void(int, const std::string&, const std::string&));
  MOCK_METHOD3(RequestAppAvailability,
               void(CastSocket* socket,
                    const std::string& app_id,
                    GetAppAvailabilityCallback callback));
  MOCK_METHOD1(RequestReceiverStatus, void(int channel_id));
  MOCK_METHOD3(SendBroadcastMessage,
               void(int,
                    const std::vector<std::string>&,
                    const BroadcastRequest&));
  MOCK_METHOD4(LaunchSession,
               void(int,
                    const std::string&,
                    base::TimeDelta,
                    LaunchSessionCallback callback));
  MOCK_METHOD4(StopSession,
               void(int channel_id,
                    const std::string& session_id,
                    const base::Optional<std::string>& client_id,
                    ResultCallback callback));
  MOCK_METHOD2(SendAppMessage,
               Result(int channel_id, const CastMessage& message));
  MOCK_METHOD4(SendMediaRequest,
               base::Optional<int>(int channel_id,
                                   const base::Value& body,
                                   const std::string& source_id,
                                   const std::string& destination_id));
  MOCK_METHOD4(SendSetVolumeRequest,
               void(int channel_id,
                    const base::Value& body,
                    const std::string& source_id,
                    ResultCallback callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastMessageHandler);
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(testing::get<cb_idx>(args), rv));
}

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_TEST_UTIL_H_
