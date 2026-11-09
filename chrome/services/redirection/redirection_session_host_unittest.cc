// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/redirection/redirection_session_host.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/services/redirection/fake_mmr_objects.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace redirection {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::NiceMock;

constexpr uint32_t kDataPipeCapacity = 1024;
constexpr int32_t kAudioDemuxerHandle = 7;
constexpr int32_t kVideoDemuxerHandle = 8;
constexpr int32_t kInvalidDemuxerHandle = -1;
constexpr char kFriendlyName[] = "Redirection";

scoped_refptr<media::DecoderBuffer> MakeFrame(std::string_view data) {
  auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span(data));
  buffer->set_timestamp(base::Microseconds(1));
  buffer->set_duration(base::Microseconds(1000));
  buffer->set_is_key_frame(true);
  return buffer;
}

// Stands in for the browser's end of the remoting source pipe.
class MockRemotingSource : public media::mojom::RemotingSource {
 public:
  mojo::PendingRemote<media::mojom::RemotingSource> BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockRemotingSource::OnDisconnected, base::Unretained(this)));
    return remote;
  }

  // False once the session in the utility process has closed its end.
  bool is_connected() const { return receiver_.is_bound(); }

  // Flushes messages in flight on this pipe.
  void FlushSourcePipe() {
    if (receiver_.is_bound()) {
      receiver_.FlushForTesting();
    }
  }

  // media::mojom::RemotingSource implementation.
  MOCK_METHOD(void, OnSinkGone, (), (override));
  MOCK_METHOD(void, OnStarted, (), (override));
  MOCK_METHOD(void,
              OnStartFailed,
              (media::mojom::RemotingStartFailReason),
              (override));
  MOCK_METHOD(void,
              OnMessageFromSink,
              (const std::vector<uint8_t>&),
              (override));
  MOCK_METHOD(void, OnStopped, (media::mojom::RemotingStopReason), (override));

  // Mojo hands the metadata over by pointer, which gmock cannot match on.
  MOCK_METHOD(void,
              OnSinkAvailable,
              (const media::mojom::RemotingSinkMetadata&));
  void OnSinkAvailable(
      media::mojom::RemotingSinkMetadataPtr metadata) override {
    OnSinkAvailable(*metadata);
  }

 private:
  void OnDisconnected() { receiver_.reset(); }

  mojo::Receiver<media::mojom::RemotingSource> receiver_{this};
};

class RedirectionSessionHostTest : public ::testing::Test {
 protected:
  enum class StreamSelection { kAudioOnly, kVideoOnly, kAudioAndVideo };

  RedirectionSessionHostTest()
      : mmr_session_(Microsoft::WRL::Make<FakeMmrSession>()) {
    media::mojom::RemotingSinkMetadata sink_metadata;
    sink_metadata.friendly_name = kFriendlyName;
    host_ = std::make_unique<RedirectionSessionHost>(
        mmr_session_.Get(), sink_metadata,
        session_host_.BindNewPipeAndPassReceiver(),
        remoter_.BindNewPipeAndPassReceiver(),
        source_.BindNewPipeAndPassRemote(), disconnected_.GetCallback());
  }

  // Flushes calls into the session before resulting source notifications.
  void FlushPipes() {
    if (session_host_.is_bound()) {
      session_host_.FlushForTesting();
    }
    if (remoter_.is_bound()) {
      remoter_.FlushForTesting();
    }
    source_.FlushSourcePipe();
  }

  // Waits for the source pipe to close and all preceding messages to arrive.
  [[nodiscard]] bool WaitForSessionEnd() {
    return base::test::RunUntil([this] { return !source_.is_connected(); });
  }

  void StartRemoting() {
    remoter_->Start();
    FlushPipes();
  }

  // Offers one data pipe and sender remote for each requested stream.
  void StartDataStreams(StreamSelection streams) {
    mojo::ScopedDataPipeConsumerHandle audio_consumer;
    mojo::ScopedDataPipeConsumerHandle video_consumer;
    if (streams != StreamSelection::kVideoOnly) {
      audio_writer_ = CreateDataPipe(audio_consumer);
    }
    if (streams != StreamSelection::kAudioOnly) {
      video_writer_ = CreateDataPipe(video_consumer);
    }
    remoter_->StartDataStreams(std::move(audio_consumer),
                               std::move(video_consumer),
                               audio_sender_.BindNewPipeAndPassReceiver(),
                               video_sender_.BindNewPipeAndPassReceiver());
    FlushPipes();
  }

  void SendAcquireDemuxer(int32_t audio_handle, int32_t video_handle) {
    openscreen::cast::RpcMessage rpc;
    rpc.set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER);
    rpc.mutable_acquire_demuxer_rpc()->set_audio_demuxer_handle(audio_handle);
    rpc.mutable_acquire_demuxer_rpc()->set_video_demuxer_handle(video_handle);
    std::string serialized;
    CHECK(rpc.SerializeToString(&serialized));
    remoter_->SendMessageToSink(
        std::vector<uint8_t>(serialized.begin(), serialized.end()));
    FlushPipes();
  }

  // Sends one frame and waits for the sender to consume it.
  void SendFrame(mojo::Remote<media::mojom::RemotingDataStreamSender>& sender,
                 media::MojoDataPipeWriter& writer,
                 std::string_view data) {
    auto buffer = MakeFrame(data);

    writer.Write(base::span(*buffer),
                 base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    base::test::TestFuture<void> frame_consumed;
    sender->SendFrame(media::mojom::DecoderBuffer::From(*buffer),
                      frame_consumed.GetCallback());
    ASSERT_TRUE(frame_consumed.Wait());
  }

  std::unique_ptr<media::MojoDataPipeWriter> CreateDataPipe(
      mojo::ScopedDataPipeConsumerHandle& consumer) {
    mojo::ScopedDataPipeProducerHandle producer;
    CHECK_EQ(mojo::CreateDataPipe(kDataPipeCapacity, producer, consumer),
             MOJO_RESULT_OK);
    return std::make_unique<media::MojoDataPipeWriter>(std::move(producer));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  Microsoft::WRL::ComPtr<FakeMmrSession> mmr_session_;
  NiceMock<MockRemotingSource> source_;
  mojo::Remote<mojom::RedirectionSessionHost> session_host_;
  mojo::Remote<media::mojom::Remoter> remoter_;
  mojo::Remote<media::mojom::RemotingDataStreamSender> audio_sender_;
  mojo::Remote<media::mojom::RemotingDataStreamSender> video_sender_;
  std::unique_ptr<media::MojoDataPipeWriter> audio_writer_;
  std::unique_ptr<media::MojoDataPipeWriter> video_writer_;
  base::test::TestFuture<RedirectionSessionHost*> disconnected_;
  std::unique_ptr<RedirectionSessionHost> host_;
};

TEST_F(RedirectionSessionHostTest, AnnouncesTheSinkOnConstruction) {
  EXPECT_CALL(source_, OnSinkAvailable(Field(
                           &media::mojom::RemotingSinkMetadata::friendly_name,
                           kFriendlyName)));

  FlushPipes();
}

TEST_F(RedirectionSessionHostTest, StartTellsTheSource) {
  EXPECT_CALL(source_, OnStarted());

  StartRemoting();
}

TEST_F(RedirectionSessionHostTest, ResponseHandlerFailureEndsTheSession) {
  mmr_session_->set_response_handler_result(E_FAIL);
  EXPECT_CALL(source_, OnStarted()).Times(0);
  EXPECT_CALL(source_,
              OnStopped(media::mojom::RemotingStopReason::MESSAGE_SEND_FAILED));

  StartRemoting();

  EXPECT_TRUE(WaitForSessionEnd());
  EXPECT_FALSE(mmr_session_->response_handler());
}

TEST_F(RedirectionSessionHostTest, MessagesFromTheSinkReachTheSource) {
  StartRemoting();

  std::vector<uint8_t> message = {1, 2, 3};
  EXPECT_CALL(source_, OnMessageFromSink(message));

  mmr_session_->SendResponse(message);
  FlushPipes();
}

TEST_F(RedirectionSessionHostTest, MessagesFromTheSourceReachTheSession) {
  StartRemoting();

  const std::vector<uint8_t> message = {4, 5, 6};
  remoter_->SendMessageToSink(message);
  FlushPipes();

  EXPECT_THAT(mmr_session_->sent_messages(), ElementsAre(message));
}

TEST_F(RedirectionSessionHostTest, StoppingEndsTheSession) {
  StartRemoting();
  ASSERT_TRUE(mmr_session_->response_handler());

  EXPECT_CALL(source_,
              OnStopped(media::mojom::RemotingStopReason::USER_DISABLED));

  remoter_->Stop(media::mojom::RemotingStopReason::USER_DISABLED);

  // Redirection cannot fall back to mirroring, so the source pipe closes.
  EXPECT_TRUE(WaitForSessionEnd());
  // Releasing the dispatcher also removes its response handler.
  EXPECT_FALSE(mmr_session_->response_handler());
}

TEST_F(RedirectionSessionHostTest, DemuxerHandlesOpenStreamsAndFramesFlow) {
  StartRemoting();
  StartDataStreams(StreamSelection::kAudioAndVideo);
  SendAcquireDemuxer(kAudioDemuxerHandle, kVideoDemuxerHandle);

  FakeMmrStream* stream = mmr_session_->GetStream(kVideoDemuxerHandle);
  ASSERT_TRUE(stream);

  SendFrame(video_sender_, *video_writer_, "video frame");

  ASSERT_EQ(stream->frames().size(), 1u);
  EXPECT_EQ(
      stream->frames()[0].data,
      base::ToVector(base::as_byte_span(std::string_view("video frame"))));
}

TEST_F(RedirectionSessionHostTest, AudioAndVideoGetSeparateStreams) {
  StartRemoting();
  StartDataStreams(StreamSelection::kAudioAndVideo);
  SendAcquireDemuxer(kAudioDemuxerHandle, kVideoDemuxerHandle);

  FakeMmrStream* audio_stream = mmr_session_->GetStream(kAudioDemuxerHandle);
  FakeMmrStream* video_stream = mmr_session_->GetStream(kVideoDemuxerHandle);
  ASSERT_TRUE(audio_stream);
  ASSERT_TRUE(video_stream);

  SendFrame(audio_sender_, *audio_writer_, "audio frame");

  EXPECT_EQ(audio_stream->frames().size(), 1u);
  EXPECT_TRUE(video_stream->frames().empty());
}

TEST_F(RedirectionSessionHostTest, NoStreamIsOpenedForAnAbsentPipe) {
  StartRemoting();
  StartDataStreams(StreamSelection::kVideoOnly);
  SendAcquireDemuxer(kAudioDemuxerHandle, kVideoDemuxerHandle);

  EXPECT_FALSE(mmr_session_->GetStream(kAudioDemuxerHandle));
  EXPECT_TRUE(mmr_session_->GetStream(kVideoDemuxerHandle));
}

TEST_F(RedirectionSessionHostTest, NoStreamIsOpenedForAnInvalidHandle) {
  StartRemoting();
  StartDataStreams(StreamSelection::kAudioAndVideo);
  SendAcquireDemuxer(kInvalidDemuxerHandle, kVideoDemuxerHandle);

  EXPECT_FALSE(mmr_session_->GetStream(kAudioDemuxerHandle));
  EXPECT_TRUE(mmr_session_->GetStream(kVideoDemuxerHandle));
}

TEST_F(RedirectionSessionHostTest, StreamCreationFailureEndsTheSession) {
  StartRemoting();
  StartDataStreams(StreamSelection::kAudioAndVideo);
  mmr_session_->set_create_stream_result(E_FAIL);
  EXPECT_CALL(source_,
              OnStopped(media::mojom::RemotingStopReason::DATA_SEND_FAILED));

  SendAcquireDemuxer(kAudioDemuxerHandle, kVideoDemuxerHandle);

  // Otherwise the source would wait indefinitely for a frame acknowledgment.
  EXPECT_TRUE(WaitForSessionEnd());
}

TEST_F(RedirectionSessionHostTest, StreamWriteFailureEndsTheSession) {
  StartRemoting();
  StartDataStreams(StreamSelection::kVideoOnly);
  SendAcquireDemuxer(kInvalidDemuxerHandle, kVideoDemuxerHandle);

  FakeMmrStream* stream = mmr_session_->GetStream(kVideoDemuxerHandle);
  ASSERT_TRUE(stream);
  stream->set_result(E_FAIL);
  EXPECT_CALL(source_,
              OnStopped(media::mojom::RemotingStopReason::DATA_SEND_FAILED));

  auto buffer = MakeFrame("video frame");
  video_writer_->Write(base::span(*buffer), base::BindOnce([](bool success) {
                         EXPECT_TRUE(success);
                       }));
  video_sender_->SendFrame(media::mojom::DecoderBuffer::From(*buffer),
                           base::DoNothing());

  EXPECT_TRUE(WaitForSessionEnd());
}

TEST_F(RedirectionSessionHostTest,
       DisconnectCallbackRunsWhenTheSessionHostPipeCloses) {
  FlushPipes();
  EXPECT_FALSE(disconnected_.IsReady());

  session_host_.reset();

  EXPECT_EQ(disconnected_.Get(), host_.get());
}

TEST_F(RedirectionSessionHostTest, DestroyedWhileRemotingDoesNotCrash) {
  StartRemoting();
  StartDataStreams(StreamSelection::kAudioAndVideo);
  SendAcquireDemuxer(kAudioDemuxerHandle, kVideoDemuxerHandle);

  host_.reset();

  EXPECT_TRUE(WaitForSessionEnd());
}

}  // namespace
}  // namespace redirection
