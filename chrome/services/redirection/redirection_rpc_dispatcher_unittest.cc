// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/redirection/redirection_rpc_dispatcher.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "chrome/services/redirection/fake_mmr_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace redirection {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr int32_t kAudioDemuxerHandle = 7;
constexpr int32_t kVideoDemuxerHandle = 8;

std::vector<uint8_t> Serialize(const openscreen::cast::RpcMessage& rpc) {
  std::vector<uint8_t> serialized(rpc.ByteSizeLong());
  CHECK(rpc.SerializeToArray(serialized.data(), serialized.size()));
  return serialized;
}

std::vector<uint8_t> AcquireDemuxerMessage() {
  openscreen::cast::RpcMessage rpc;
  rpc.set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER);
  rpc.mutable_acquire_demuxer_rpc()->set_audio_demuxer_handle(
      kAudioDemuxerHandle);
  rpc.mutable_acquire_demuxer_rpc()->set_video_demuxer_handle(
      kVideoDemuxerHandle);
  return Serialize(rpc);
}

std::vector<uint8_t> InitializeCallbackMessage() {
  openscreen::cast::RpcMessage rpc;
  rpc.set_proc(openscreen::cast::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
  rpc.set_boolean_value(true);
  return Serialize(rpc);
}

class RedirectionRpcDispatcherTest : public ::testing::Test {
 protected:
  RedirectionRpcDispatcherTest()
      : session_(Microsoft::WRL::Make<FakeMmrSession>()) {
    dispatcher_ = std::make_unique<RedirectionRpcDispatcher>(
        session_.Get(),
        base::BindLambdaForTesting([this](int32_t audio, int32_t video) {
          // The session must already have seen the message that carried these
          // handles, since opening the matching streams depends on it.
          messages_seen_when_handles_arrived_ =
              session_->sent_messages().size();
          demuxer_handles_.SetValue(std::make_pair(audio, video));
        }));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  Microsoft::WRL::ComPtr<FakeMmrSession> session_;
  std::unique_ptr<RedirectionRpcDispatcher> dispatcher_;
  base::test::TestFuture<std::pair<int32_t, int32_t>> demuxer_handles_;
  size_t messages_seen_when_handles_arrived_ = 0;
};

TEST_F(RedirectionRpcDispatcherTest, SubscribeRoutesResponsesToTheCallback) {
  base::test::TestFuture<std::vector<uint8_t>> response;
  dispatcher_->Subscribe(
      response.GetRepeatingCallback<const std::vector<uint8_t>&>(),
      base::DoNothing());
  ASSERT_TRUE(session_->response_handler());

  std::vector<uint8_t> message = InitializeCallbackMessage();
  session_->SendResponse(message);

  EXPECT_EQ(response.Take(), message);
}

TEST_F(RedirectionRpcDispatcherTest,
       ResponseFromBackgroundThreadRunsOnSubscribingSequence) {
  scoped_refptr<base::SingleThreadTaskRunner> subscribing_task_runner =
      task_environment_.GetMainThreadTaskRunner();
  base::test::TestFuture<void> response_received;
  bool ran_on_subscribing_sequence = false;
  std::vector<uint8_t> received_message;
  dispatcher_->Subscribe(
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& message) {
        ran_on_subscribing_sequence =
            subscribing_task_runner->RunsTasksInCurrentSequence();
        received_message = message;
        response_received.SetValue();
      }),
      base::DoNothing());

  Microsoft::WRL::ComPtr<IMMRResponseHandler> response_handler =
      session_->response_handler();
  ASSERT_TRUE(response_handler);
  std::vector<uint8_t> message = InitializeCallbackMessage();
  base::Thread mmr_thread("MMR response thread");
  ASSERT_TRUE(mmr_thread.Start());
  mmr_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](Microsoft::WRL::ComPtr<IMMRResponseHandler> handler,
                        std::vector<uint8_t> response) {
                       handler->OnResponse(
                           response.data(),
                           static_cast<UINT32>(response.size()));
                     },
                     std::move(response_handler), message));

  ASSERT_TRUE(response_received.Wait());
  EXPECT_TRUE(ran_on_subscribing_sequence);
  EXPECT_EQ(received_message, message);
}

TEST_F(RedirectionRpcDispatcherTest, SubscribeFailureIsReported) {
  session_->set_response_handler_result(E_FAIL);
  base::test::TestFuture<void> error;

  dispatcher_->Subscribe(base::DoNothing(), error.GetRepeatingCallback());

  EXPECT_TRUE(error.Wait());
  EXPECT_FALSE(session_->response_handler());
}

TEST_F(RedirectionRpcDispatcherTest, UnsubscribeClearsTheSessionHandler) {
  dispatcher_->Subscribe(base::DoNothing(), base::DoNothing());
  ASSERT_TRUE(session_->response_handler());

  dispatcher_->Unsubscribe();

  EXPECT_FALSE(session_->response_handler());
}

TEST_F(RedirectionRpcDispatcherTest, UnsubscribeDropsQueuedResponses) {
  base::test::TestFuture<std::vector<uint8_t>> response;
  dispatcher_->Subscribe(
      response.GetRepeatingCallback<const std::vector<uint8_t>&>(),
      base::DoNothing());

  std::vector<uint8_t> message = InitializeCallbackMessage();
  session_->SendResponse(message);
  dispatcher_->Unsubscribe();
  base::test::TestFuture<void> queue_drained;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, queue_drained.GetCallback());
  ASSERT_TRUE(queue_drained.Wait());

  EXPECT_FALSE(response.IsReady());
}

TEST_F(RedirectionRpcDispatcherTest,
       ResubscribeDropsResponsesFromPreviousSubscription) {
  base::test::TestFuture<std::vector<uint8_t>> first_response;
  dispatcher_->Subscribe(
      first_response.GetRepeatingCallback<const std::vector<uint8_t>&>(),
      base::DoNothing());
  std::vector<uint8_t> first_message = {1};
  session_->SendResponse(first_message);

  dispatcher_->Unsubscribe();

  base::test::TestFuture<std::vector<uint8_t>> second_response;
  dispatcher_->Subscribe(
      second_response.GetRepeatingCallback<const std::vector<uint8_t>&>(),
      base::DoNothing());
  std::vector<uint8_t> second_message = {2};
  session_->SendResponse(second_message);

  EXPECT_EQ(second_response.Take(), second_message);
  EXPECT_FALSE(first_response.IsReady());
}

TEST_F(RedirectionRpcDispatcherTest, DestructionClearsTheSessionHandler) {
  dispatcher_->Subscribe(base::DoNothing(), base::DoNothing());
  ASSERT_TRUE(session_->response_handler());

  dispatcher_.reset();

  EXPECT_FALSE(session_->response_handler());
}

TEST_F(RedirectionRpcDispatcherTest, OutboundMessagesReachTheSession) {
  const std::vector<uint8_t> message = InitializeCallbackMessage();

  EXPECT_TRUE(dispatcher_->SendOutboundMessage(message));

  EXPECT_THAT(session_->sent_messages(), ElementsAre(message));
  EXPECT_FALSE(demuxer_handles_.IsReady());
}

TEST_F(RedirectionRpcDispatcherTest, OutboundMessageFailureIsReported) {
  base::test::TestFuture<void> error;
  dispatcher_->Subscribe(base::DoNothing(), error.GetRepeatingCallback());
  session_->set_send_message_result(E_FAIL);

  EXPECT_FALSE(dispatcher_->SendOutboundMessage(InitializeCallbackMessage()));

  EXPECT_TRUE(error.Wait());
  EXPECT_THAT(session_->sent_messages(), IsEmpty());
}

TEST_F(RedirectionRpcDispatcherTest,
       DemuxerHandlesAreReportedAfterTheSessionSeesTheMessage) {
  ASSERT_TRUE(dispatcher_->SendOutboundMessage(AcquireDemuxerMessage()));

  EXPECT_EQ(demuxer_handles_.Take(),
            std::make_pair(kAudioDemuxerHandle, kVideoDemuxerHandle));
  EXPECT_EQ(messages_seen_when_handles_arrived_, 1u);
}

TEST_F(RedirectionRpcDispatcherTest, DemuxerHandlesAreReportedOnlyOnce) {
  const std::vector<uint8_t> message = AcquireDemuxerMessage();
  ASSERT_TRUE(dispatcher_->SendOutboundMessage(message));
  ASSERT_TRUE(demuxer_handles_.Wait());

  // A second RPC_ACQUIRE_DEMUXER must not re-run the callback, which would
  // otherwise reopen streams the session is already writing to.
  demuxer_handles_.Clear();
  ASSERT_TRUE(dispatcher_->SendOutboundMessage(message));

  EXPECT_FALSE(demuxer_handles_.IsReady());
  EXPECT_EQ(session_->sent_messages().size(), 2u);
}

TEST_F(RedirectionRpcDispatcherTest, UnparseableMessagesAreStillForwarded) {
  // The dispatcher inspects messages only to spot RPC_ACQUIRE_DEMUXER; it is
  // not the arbiter of what is valid on the wire.
  const std::vector<uint8_t> garbage = {0xff, 0xff, 0xff};

  EXPECT_TRUE(dispatcher_->SendOutboundMessage(garbage));

  EXPECT_THAT(session_->sent_messages(), ElementsAre(garbage));
  EXPECT_FALSE(demuxer_handles_.IsReady());
}

}  // namespace
}  // namespace redirection
