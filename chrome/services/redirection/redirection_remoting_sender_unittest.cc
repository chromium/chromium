// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/redirection/redirection_remoting_sender.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/services/redirection/fake_mmr_objects.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace redirection {
namespace {

constexpr uint32_t kDataPipeCapacity = 1024;

// IMMRStream expresses times in Windows 100-nanosecond units.
constexpr int64_t kHnsPerMicrosecond = 10;

std::vector<uint8_t> ToBytes(std::string_view data) {
  return base::ToVector(base::as_byte_span(data));
}

struct FrameParameters {
  std::string_view data;
  int64_t timestamp_us = 0;
  bool is_key_frame = false;
};

scoped_refptr<media::DecoderBuffer> MakeFrame(const FrameParameters& params) {
  auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span(params.data));
  buffer->set_timestamp(base::Microseconds(params.timestamp_us));
  buffer->set_duration(base::Microseconds(1000));
  buffer->set_is_key_frame(params.is_key_frame);
  return buffer;
}

class RedirectionRemotingSenderTest : public ::testing::Test {
 protected:
  RedirectionRemotingSenderTest()
      : stream_(Microsoft::WRL::Make<FakeMmrStream>()) {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    CHECK_EQ(mojo::CreateDataPipe(kDataPipeCapacity, producer, consumer),
             MOJO_RESULT_OK);
    data_pipe_writer_ =
        std::make_unique<media::MojoDataPipeWriter>(std::move(producer));
    sender_ = std::make_unique<RedirectionRemotingSender>(
        std::move(consumer), stream_sender_.BindNewPipeAndPassReceiver(),
        error_.GetCallback());
  }

  // Hands `buffer` to the sender the way the renderer does, without waiting for
  // the sender to consume it.
  void SendFrameAsync(const scoped_refptr<media::DecoderBuffer>& buffer,
                      base::OnceClosure frame_consumed) {
    if (!buffer->end_of_stream()) {
      data_pipe_writer_->Write(
          base::span(*buffer),
          base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    }

    stream_sender_->SendFrame(media::mojom::DecoderBuffer::From(*buffer),
                              std::move(frame_consumed));
  }

  // Hands `buffer` to the sender the way the renderer does, and returns once
  // the sender has consumed it.
  void SendFrame(const scoped_refptr<media::DecoderBuffer>& buffer) {
    base::test::TestFuture<void> frame_consumed;
    SendFrameAsync(buffer, frame_consumed.GetCallback());
    ASSERT_TRUE(frame_consumed.Wait());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  Microsoft::WRL::ComPtr<FakeMmrStream> stream_;
  std::unique_ptr<media::MojoDataPipeWriter> data_pipe_writer_;
  mojo::Remote<media::mojom::RemotingDataStreamSender> stream_sender_;
  base::test::TestFuture<void> error_;
  std::unique_ptr<RedirectionRemotingSender> sender_;
};

TEST_F(RedirectionRemotingSenderTest, FramesAreWrittenToTheStreamVerbatim) {
  sender_->SetStream(stream_.Get());

  SendFrame(MakeFrame(
      {.data = "frame data", .timestamp_us = 1234, .is_key_frame = true}));

  ASSERT_EQ(stream_->frames().size(), 1u);
  const FakeMmrStream::Frame& frame = stream_->frames()[0];
  EXPECT_EQ(frame.data, ToBytes("frame data"));
  EXPECT_EQ(frame.timestamp_hns, 1234 * kHnsPerMicrosecond);
  EXPECT_EQ(frame.duration_hns, 1000 * kHnsPerMicrosecond);
  EXPECT_TRUE(frame.is_key_frame);
}

TEST_F(RedirectionRemotingSenderTest, NonKeyFramesAreMarkedAsSuch) {
  sender_->SetStream(stream_.Get());

  SendFrame(
      MakeFrame({.data = "delta", .timestamp_us = 1, .is_key_frame = false}));

  ASSERT_EQ(stream_->frames().size(), 1u);
  EXPECT_FALSE(stream_->frames()[0].is_key_frame);
}

TEST_F(RedirectionRemotingSenderTest, UnknownDurationIsSentAsZero) {
  sender_->SetStream(stream_.Get());
  auto buffer = media::DecoderBuffer::CopyFrom(
      base::as_byte_span(std::string_view("frame")));
  buffer->set_timestamp(base::TimeDelta());
  buffer->set_duration(media::kNoTimestamp);

  SendFrame(buffer);

  ASSERT_EQ(stream_->frames().size(), 1u);
  EXPECT_EQ(stream_->frames()[0].duration_hns, 0);
}

TEST_F(RedirectionRemotingSenderTest,
       FramesSentBeforeTheStreamExistsAreDeliveredInOrder) {
  // The sink names its stream ids over RPC, so the renderer can issue
  // SendFrame() before there is anywhere to put the frames. The frame is held
  // until SetStream() requests the first read.
  base::test::TestFuture<void> first_consumed;
  SendFrameAsync(
      MakeFrame({.data = "first", .timestamp_us = 1, .is_key_frame = true}),
      first_consumed.GetCallback());
  stream_sender_.FlushForTesting();
  EXPECT_TRUE(stream_->frames().empty());
  EXPECT_FALSE(first_consumed.IsReady());

  sender_->SetStream(stream_.Get());
  ASSERT_TRUE(first_consumed.Wait());

  SendFrame(
      MakeFrame({.data = "second", .timestamp_us = 2, .is_key_frame = false}));

  ASSERT_EQ(stream_->frames().size(), 2u);
  EXPECT_EQ(stream_->frames()[0].data, ToBytes("first"));
  EXPECT_EQ(stream_->frames()[1].data, ToBytes("second"));
}

TEST_F(RedirectionRemotingSenderTest,
       CancelDiscardsFrameSentBeforeTheStreamExists) {
  base::test::TestFuture<void> frame_consumed;
  SendFrameAsync(
      MakeFrame({.data = "stale", .timestamp_us = 1, .is_key_frame = true}),
      frame_consumed.GetCallback());
  stream_sender_->CancelInFlightData();
  stream_sender_.FlushForTesting();
  EXPECT_FALSE(frame_consumed.IsReady());

  sender_->SetStream(stream_.Get());
  ASSERT_TRUE(frame_consumed.Wait());
  EXPECT_TRUE(stream_->frames().empty());

  SendFrame(
      MakeFrame({.data = "current", .timestamp_us = 2, .is_key_frame = true}));
  ASSERT_EQ(stream_->frames().size(), 1u);
  EXPECT_EQ(stream_->frames()[0].data, ToBytes("current"));
}

TEST_F(RedirectionRemotingSenderTest, CancelDiscardsFrameDuringPipeRead) {
  sender_->SetStream(stream_.Get());
  const auto stale_frame =
      MakeFrame({.data = "stale", .timestamp_us = 1, .is_key_frame = true});
  base::test::TestFuture<void> frame_consumed;
  stream_sender_->SendFrame(media::mojom::DecoderBuffer::From(*stale_frame),
                            frame_consumed.GetCallback());
  stream_sender_->CancelInFlightData();
  stream_sender_.FlushForTesting();
  EXPECT_FALSE(frame_consumed.IsReady());

  data_pipe_writer_->Write(
      base::span(*stale_frame),
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ASSERT_TRUE(frame_consumed.Wait());
  EXPECT_TRUE(stream_->frames().empty());

  SendFrame(
      MakeFrame({.data = "current", .timestamp_us = 2, .is_key_frame = true}));
  ASSERT_EQ(stream_->frames().size(), 1u);
  EXPECT_EQ(stream_->frames()[0].data, ToBytes("current"));
}

TEST_F(RedirectionRemotingSenderTest, EndOfStreamIsForwarded) {
  sender_->SetStream(stream_.Get());

  SendFrame(media::DecoderBuffer::CreateEOSBuffer());

  EXPECT_TRUE(stream_->frames().empty());
  EXPECT_EQ(stream_->end_of_stream_count(), 1);
}

TEST_F(RedirectionRemotingSenderTest, FramesCanResumeAfterEndOfStream) {
  sender_->SetStream(stream_.Get());
  SendFrame(media::DecoderBuffer::CreateEOSBuffer());

  stream_sender_->CancelInFlightData();
  stream_sender_.FlushForTesting();
  SendFrame(MakeFrame(
      {.data = "restarted", .timestamp_us = 0, .is_key_frame = true}));

  EXPECT_EQ(stream_->end_of_stream_count(), 1);
  ASSERT_EQ(stream_->frames().size(), 1u);
  EXPECT_EQ(stream_->frames()[0].data, ToBytes("restarted"));
}

TEST_F(RedirectionRemotingSenderTest, EndOfStreamSentBeforeTheStreamExists) {
  base::test::TestFuture<void> frame_consumed;
  SendFrameAsync(media::DecoderBuffer::CreateEOSBuffer(),
                 frame_consumed.GetCallback());
  stream_sender_.FlushForTesting();
  EXPECT_EQ(stream_->end_of_stream_count(), 0);

  sender_->SetStream(stream_.Get());
  ASSERT_TRUE(frame_consumed.Wait());

  EXPECT_EQ(stream_->end_of_stream_count(), 1);
}

TEST_F(RedirectionRemotingSenderTest, CanceledEndOfStreamIsDiscarded) {
  base::test::TestFuture<void> end_of_stream_consumed;
  SendFrameAsync(media::DecoderBuffer::CreateEOSBuffer(),
                 end_of_stream_consumed.GetCallback());
  stream_sender_->CancelInFlightData();
  stream_sender_.FlushForTesting();

  sender_->SetStream(stream_.Get());
  ASSERT_TRUE(end_of_stream_consumed.Wait());
  EXPECT_EQ(stream_->end_of_stream_count(), 0);

  SendFrame(
      MakeFrame({.data = "current", .timestamp_us = 1, .is_key_frame = true}));
  ASSERT_EQ(stream_->frames().size(), 1u);
  EXPECT_EQ(stream_->frames()[0].data, ToBytes("current"));
}

TEST_F(RedirectionRemotingSenderTest, SourceGoingAwayIsReportedAsAnError) {
  sender_->SetStream(stream_.Get());

  stream_sender_.reset();

  EXPECT_TRUE(error_.Wait());
}

TEST_F(RedirectionRemotingSenderTest, SourceGoingAwayBeforeStreamExists) {
  stream_sender_.reset();
  ASSERT_TRUE(error_.Wait());

  sender_->SetStream(stream_.Get());
}

TEST_F(RedirectionRemotingSenderTest, AppendBufferFailureIsReportedAsAnError) {
  sender_->SetStream(stream_.Get());
  stream_->set_result(E_FAIL);

  base::test::TestFuture<void> frame_consumed;
  SendFrameAsync(
      MakeFrame({.data = "frame", .timestamp_us = 1, .is_key_frame = true}),
      frame_consumed.GetCallback());

  EXPECT_TRUE(error_.Wait());
  // A frame the sink never took must not be acked, or the renderer would send
  // the next one into a dead session.
  EXPECT_FALSE(frame_consumed.IsReady());
}

TEST_F(RedirectionRemotingSenderTest, EndOfStreamFailureIsReportedAsAnError) {
  sender_->SetStream(stream_.Get());
  stream_->set_result(E_FAIL);

  base::test::TestFuture<void> frame_consumed;
  SendFrameAsync(media::DecoderBuffer::CreateEOSBuffer(),
                 frame_consumed.GetCallback());

  EXPECT_TRUE(error_.Wait());
  EXPECT_FALSE(frame_consumed.IsReady());
}

}  // namespace
}  // namespace redirection
