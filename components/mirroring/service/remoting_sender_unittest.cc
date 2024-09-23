// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/remoting_sender.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "media/cast/common/encoded_frame.h"
#include "media/cast/common/frame_id.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/cast/sender/frame_sender.h"
#include "media/cast/test/utility/default_config.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace mirroring {
namespace {

void AreEqualExceptKeyframeImpl(const media::cast::SenderEncodedFrame& frame,
                                const media::DecoderBuffer& buffer,
                                base::test::TaskEnvironment& environment) {
  if (buffer.is_key_frame()) {
    EXPECT_EQ(frame.dependency, Dependency::kKeyFrame);
  }

  scoped_refptr<media::DecoderBuffer> received_buffer =
      media::cast::ByteArrayToDecoderBuffer(base::as_byte_span(frame.data));
  EXPECT_EQ(base::span(buffer), base::span(*received_buffer));

  // The reference time and encode completion time should be the same, and
  // is based on the clock time.
  EXPECT_EQ(frame.reference_time, frame.encode_completion_time);

  // The reference time should use the current environment time. Since we are
  // using a mock clock that is only manually advanced, it should be equal to
  // the environment's current time.
  EXPECT_EQ(frame.reference_time, environment.NowTicks());
}

ACTION_P(AreEqualNotFirstFrame, buffer, env) {
  AreEqualExceptKeyframeImpl(*arg0, *buffer, *env);
  return media::cast::CastStreamingFrameDropReason::kNotDropped;
}

ACTION_P(AreEqualFirstFrame, buffer, env) {
  EXPECT_EQ(arg0->dependency, Dependency::kKeyFrame);
  AreEqualExceptKeyframeImpl(*arg0, *buffer, *env);
  return media::cast::CastStreamingFrameDropReason::kNotDropped;
}

// Data pipe capacity is 1KB.
constexpr int kDataPipeCapacity = 1024;

class FakeSender : public media::cast::FrameSender {
 public:
  ~FakeSender() override = default;

  MOCK_METHOD1(SetTargetPlayoutDelay, void(base::TimeDelta));
  MOCK_CONST_METHOD0(GetTargetPlayoutDelay, base::TimeDelta());
  MOCK_CONST_METHOD0(NeedsKeyFrame, bool());
  MOCK_METHOD1(EnqueueFrame,
               media::cast::CastStreamingFrameDropReason(
                   std::unique_ptr<media::cast::SenderEncodedFrame>));
  MOCK_METHOD1(ShouldDropNextFrame,
               media::cast::CastStreamingFrameDropReason(base::TimeDelta));
  MOCK_CONST_METHOD1(GetRecordedRtpTimestamp,
                     media::cast::RtpTimeTicks(media::cast::FrameId));
  MOCK_CONST_METHOD0(GetUnacknowledgedFrameCount, int());
  MOCK_METHOD2(GetSuggestedBitrate, int(base::TimeTicks, base::TimeDelta));
  MOCK_CONST_METHOD0(MaxFrameRate, double());
  MOCK_METHOD1(SetMaxFrameRate, void(double));
  MOCK_CONST_METHOD0(TargetPlayoutDelay, base::TimeDelta());
  MOCK_CONST_METHOD0(CurrentRoundTripTime, base::TimeDelta());
  MOCK_CONST_METHOD0(LastSendTime, base::TimeTicks());
  MOCK_CONST_METHOD0(LastAckedFrameId, media::cast::FrameId());
};

class MojoSenderWrapper {
 public:
  MojoSenderWrapper(
      mojo::ScopedDataPipeProducerHandle handle,
      mojo::PendingRemote<media::mojom::RemotingDataStreamSender> sender)
      : data_pipe_writer_(std::move(handle)),
        stream_sender_(std::move(sender)) {}

  void SendFrame(scoped_refptr<media::DecoderBuffer> buffer) {
    SendFrame(std::move(buffer), base::OnceCallback<void()>{});
  }

  void SendFrame(scoped_refptr<media::DecoderBuffer> buffer,
                 base::OnceCallback<void()> on_read_complete) {
    ASSERT_FALSE(is_frame_in_flight_);
    is_frame_in_flight_ = true;

    data_pipe_writer_.Write(
        buffer->data(), buffer->size(),
        base::BindOnce(&MojoSenderWrapper::OnPipeWriteComplete,
                       weak_factory_.GetWeakPtr()));
    stream_sender_->SendFrame(
        media::mojom::DecoderBuffer::From(*buffer),
        base::BindOnce(&MojoSenderWrapper::OnFrameReadComplete,
                       weak_factory_.GetWeakPtr(),
                       std::move(on_read_complete)));
  }

  void CancelInFlightData() { stream_sender_->CancelInFlightData(); }

  bool is_frame_in_flight() const { return is_frame_in_flight_; }

 private:
  void OnFrameReadComplete(base::OnceCallback<void()> on_read_complete) {
    is_frame_in_flight_ = false;
    if (on_read_complete) {
      std::move(on_read_complete).Run();
    }
  }

  void OnPipeWriteComplete(bool success) { ASSERT_TRUE(success); }

  bool is_frame_in_flight_ = false;

  media::MojoDataPipeWriter data_pipe_writer_;
  mojo::Remote<media::mojom::RemotingDataStreamSender> stream_sender_;

  base::WeakPtrFactory<MojoSenderWrapper> weak_factory_{this};
};

}  // namespace

class RemotingSenderTest : public ::testing::Test {
 public:
  RemotingSenderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        cast_environment_(new media::cast::CastEnvironment(
            task_environment_.GetMockTickClock(),
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner())) {
    media::cast::FrameSenderConfig video_config =
        media::cast::GetDefaultVideoSenderConfig();
    std::unique_ptr<testing::StrictMock<FakeSender>> fake_sender =
        std::make_unique<testing::StrictMock<FakeSender>>();
    sender_ = fake_sender.get();

    mojo::PendingRemote<media::mojom::RemotingDataStreamSender> sender;
    const MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        kDataPipeCapacity};
    mojo::ScopedDataPipeProducerHandle producer_end;
    mojo::ScopedDataPipeConsumerHandle consumer_end;
    CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&data_pipe_options,
                                                  producer_end, consumer_end));

    remoting_sender_ = base::WrapUnique(new RemotingSender(
        cast_environment_, std::move(fake_sender), video_config,
        std::move(consumer_end), sender.InitWithNewPipeAndPassReceiver(),
        base::BindOnce(
            [](bool expecting_error_callback_run) {
              CHECK(expecting_error_callback_run);
            },
            expecting_error_callback_run_)));

    mojo_sender_wrapper_ = std::make_unique<MojoSenderWrapper>(
        std::move(producer_end), std::move(sender));

    std::vector<uint8_t> data = {1, 2, 3};
    first_buffer_ = media::DecoderBuffer::CopyFrom(data);
    first_buffer_->set_duration(base::Seconds(1));
    first_buffer_->set_timestamp(base::Seconds(2));
    first_buffer_->set_is_key_frame(false);

    data = {42, 43, 44};
    second_buffer_ = media::DecoderBuffer::CopyFrom(data);
    second_buffer_->set_duration(base::Seconds(32));
    second_buffer_->set_timestamp(base::Seconds(42));
    second_buffer_->set_is_key_frame(false);

    data = {7, 8, 9};
    third_buffer_ = media::DecoderBuffer::CopyFrom(data);
    third_buffer_->set_duration(base::Seconds(10));
    third_buffer_->set_timestamp(base::Seconds(11));
    third_buffer_->set_is_key_frame(true);
  }

  void TearDown() final {
    remoting_sender_.reset();

    // Allow any pending tasks to run before destruction.
    RunPendingTasks();
  }

 protected:
  // Allow pending tasks, such as Mojo method calls, to execute.
  void RunPendingTasks() { task_environment_.RunUntilIdle(); }

  void SendFrameCancelled(media::cast::FrameId id) {
    remoting_sender_->OnFrameCanceled(id);
  }

  base::test::TaskEnvironment task_environment_;
  const scoped_refptr<media::cast::CastEnvironment> cast_environment_;

  raw_ptr<testing::StrictMock<FakeSender>, DanglingUntriaged> sender_;
  bool expecting_error_callback_run_ = false;

  std::unique_ptr<MojoSenderWrapper> mojo_sender_wrapper_;

  std::unique_ptr<RemotingSender> remoting_sender_;

  scoped_refptr<media::DecoderBuffer> first_buffer_;
  scoped_refptr<media::DecoderBuffer> second_buffer_;
  scoped_refptr<media::DecoderBuffer> third_buffer_;

  media::cast::FrameId first_frame_id_ = media::cast::FrameId::first();
};

TEST_F(RemotingSenderTest, SendsFramesViaMojoDataPipe) {
  EXPECT_CALL(*sender_, LastSendTime)
      .WillRepeatedly(Return(base::TimeTicks::Now() - base::Milliseconds(100)));

  EXPECT_CALL(*sender_, GetRecordedRtpTimestamp(media::cast::FrameId(0)))
      .WillRepeatedly(Return(media::cast::RtpTimeTicks(1111)));
  EXPECT_CALL(*sender_, GetRecordedRtpTimestamp(media::cast::FrameId(1)))
      .WillRepeatedly(Return(media::cast::RtpTimeTicks(2222)));

  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount).WillOnce(Return(0));
  EXPECT_CALL(*sender_, EnqueueFrame(_))
      .WillOnce(AreEqualFirstFrame(first_buffer_, &task_environment_));
  mojo_sender_wrapper_->SendFrame(first_buffer_);
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());
  RunPendingTasks();
  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());

  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount).WillOnce(Return(0));
  EXPECT_CALL(*sender_, EnqueueFrame(_))
      .WillOnce(AreEqualNotFirstFrame(second_buffer_, &task_environment_));
  mojo_sender_wrapper_->SendFrame(second_buffer_);
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());
  RunPendingTasks();
  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());

  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount).WillOnce(Return(0));
  EXPECT_CALL(*sender_, EnqueueFrame(_))
      .WillOnce(AreEqualNotFirstFrame(third_buffer_, &task_environment_));
  mojo_sender_wrapper_->SendFrame(third_buffer_);
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());
  RunPendingTasks();
  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());
}

TEST_F(RemotingSenderTest, CancelsUnsentFrame) {
  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount)
      .WillOnce(Return(media::cast::kMaxUnackedFrames));
  mojo_sender_wrapper_->SendFrame(first_buffer_);
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());
  RunPendingTasks();
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());

  mojo_sender_wrapper_->CancelInFlightData();
  RunPendingTasks();
  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());

  SendFrameCancelled(first_frame_id_);
  RunPendingTasks();
  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());
}

TEST_F(RemotingSenderTest, CancelsOrAcksFramesInFlight) {
  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount)
      .WillOnce(Return(media::cast::kMaxUnackedFrames));
  mojo_sender_wrapper_->SendFrame(first_buffer_);
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());
  RunPendingTasks();
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());

  EXPECT_CALL(*sender_, EnqueueFrame(_))
      .WillOnce(AreEqualFirstFrame(first_buffer_, &task_environment_));
  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount)
      .WillOnce(Return(media::cast::kMaxUnackedFrames - 1));
  SendFrameCancelled(first_frame_id_);
  RunPendingTasks();
  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());
}

TEST_F(RemotingSenderTest, FramesWaitWhenEnqueueFails) {
  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount).WillOnce(Return(0));
  EXPECT_CALL(*sender_, EnqueueFrame(_))
      .WillOnce(Return(
          media::cast::CastStreamingFrameDropReason::kTooManyFramesInFlight));
  mojo_sender_wrapper_->SendFrame(first_buffer_);
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());
  RunPendingTasks();
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());

  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount).WillOnce(Return(0));
  EXPECT_CALL(*sender_, EnqueueFrame(_))
      .WillOnce(AreEqualFirstFrame(first_buffer_, &task_environment_));
  SendFrameCancelled(first_frame_id_);
  RunPendingTasks();
  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());
}

TEST_F(RemotingSenderTest, FramesDroppedWhenEnqueueFailsRepeatedly) {
  int enqueue_attempts = 0;

  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount).WillOnce(Return(0));
  EXPECT_CALL(*sender_, EnqueueFrame(_))
      .WillOnce(Return(
          media::cast::CastStreamingFrameDropReason::kTooManyFramesInFlight));
  mojo_sender_wrapper_->SendFrame(first_buffer_);
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());
  RunPendingTasks();
  EXPECT_TRUE(mojo_sender_wrapper_->is_frame_in_flight());
  enqueue_attempts++;

  constexpr int kMaxEnqueueFrameFailures = 3;
  for (; enqueue_attempts <= kMaxEnqueueFrameFailures; enqueue_attempts++) {
    EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount).WillOnce(Return(0));
    EXPECT_CALL(*sender_, EnqueueFrame(_))
        .WillOnce(Return(
            media::cast::CastStreamingFrameDropReason::kTooManyFramesInFlight));
    SendFrameCancelled(first_frame_id_);
    RunPendingTasks();
  }

  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());
  RunPendingTasks();
}

TEST_F(RemotingSenderTest, FramesDroppedWhenEnqueueFailsDueToFormatting) {
  EXPECT_CALL(*sender_, GetUnacknowledgedFrameCount).WillOnce(Return(0));
  EXPECT_CALL(*sender_, EnqueueFrame(_))
      .WillOnce(
          Return(media::cast::CastStreamingFrameDropReason::kPayloadTooLarge));
  mojo_sender_wrapper_->SendFrame(first_buffer_);
  RunPendingTasks();
  EXPECT_FALSE(mojo_sender_wrapper_->is_frame_in_flight());
}

}  // namespace mirroring
