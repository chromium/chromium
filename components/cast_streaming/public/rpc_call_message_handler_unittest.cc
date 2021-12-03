// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/public/rpc_call_message_handler.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace cast_streaming {
namespace remoting {

class RpcCallMessageHandlerTest : public testing::Test {
 public:
  class MockRpcCallMessageHandler : public RpcCallMessageHandler {
   protected:
   public:
    ~MockRpcCallMessageHandler() override = default;

    MOCK_METHOD0(OnRpcInitialize, void());
    MOCK_METHOD2(OnRpcFlush, void(uint32_t, uint32_t));
    MOCK_METHOD1(OnRpcStartPlayingFrom, void(base::TimeDelta));
    MOCK_METHOD1(OnRpcSetPlaybackRate, void(double));
    MOCK_METHOD1(OnRpcSetVolume, void(double));
  };

  MockRpcCallMessageHandler client_;
};

TEST_F(RpcCallMessageHandlerTest, OnRpcInitialize) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_INITIALIZE);
  EXPECT_CALL(client_, OnRpcInitialize());
  DispatchRpcCall(std::move(rpc), &client_);
}

TEST_F(RpcCallMessageHandlerTest, OnRpcFlush) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL);
  auto* flush_command = rpc->mutable_renderer_flushuntil_rpc();
  flush_command->set_audio_count(32);
  flush_command->set_video_count(42);
  EXPECT_CALL(client_, OnRpcFlush(32, 42));
  DispatchRpcCall(std::move(rpc), &client_);
}

TEST_F(RpcCallMessageHandlerTest, OnRpcStartPlayingFrom) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_STARTPLAYINGFROM);
  rpc->set_integer64_value(42);
  EXPECT_CALL(client_, OnRpcStartPlayingFrom(base::Microseconds(42)));
  DispatchRpcCall(std::move(rpc), &client_);
}

TEST_F(RpcCallMessageHandlerTest, OnRpcSetPlaybackRate) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETPLAYBACKRATE);
  rpc->set_double_value(112358.13);
  EXPECT_CALL(client_, OnRpcSetPlaybackRate(112358.13));
  DispatchRpcCall(std::move(rpc), &client_);
}

TEST_F(RpcCallMessageHandlerTest, OnRpcSetVolume) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETVOLUME);
  rpc->set_double_value(112358.13);
  EXPECT_CALL(client_, OnRpcSetVolume(112358.13));
  DispatchRpcCall(std::move(rpc), &client_);
}

}  // namespace remoting
}  // namespace cast_streaming
