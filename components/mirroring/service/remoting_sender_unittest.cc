// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/remoting_sender.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "media/cast/common/encoded_frame.h"
#include "media/cast/constants.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/test/utility/default_config.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

namespace mirroring {

namespace {

// Data pipe capacity is 1KB.
constexpr int kDataPipeCapacity = 1024;

// Implements the CastTransport interface to capture output from the
// RemotingSender.
class FakeTransport final : public media::cast::CastTransport {
 public:
  FakeTransport() = default;

  FakeTransport(const FakeTransport&) = delete;
  FakeTransport& operator=(const FakeTransport&) = delete;

  ~FakeTransport() override = default;

  void TakeSentFrames(std::vector<media::cast::EncodedFrame>* frames) {
    frames->swap(sent_frames_);
    sent_frames_.clear();
  }

  void TakeCanceledFrameIds(std::vector<media::cast::FrameId>* frame_ids) {
    frame_ids->swap(canceled_frame_ids_);
    canceled_frame_ids_.clear();
  }

  media::cast::FrameId WaitForKickstart() {
    base::RunLoop run_loop;
    kickstarted_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    return kickstarted_frame_id_;
  }

 protected:
  void InsertFrame(uint32_t ssrc,
                   const media::cast::EncodedFrame& frame) override {
    sent_frames_.push_back(frame);
  }

  void CancelSendingFrames(
      uint32_t ssrc,
      const std::vector<media::cast::FrameId>& frame_ids) override {
    for (media::cast::FrameId frame_id : frame_ids)
      canceled_frame_ids_.push_back(frame_id);
  }

  void ResendFrameForKickstart(uint32_t ssrc,
                               media::cast::FrameId frame_id) override {
    kickstarted_frame_id_ = frame_id;
    if (!kickstarted_callback_.is_null())
      std::move(kickstarted_callback_).Run();
  }

  // The rest of the interface is not used for these tests.
  void SendSenderReport(
      uint32_t ssrc,
      base::TimeTicks current_time,
      media::cast::RtpTimeTicks current_time_as_rtp_timestamp) override {}
  void AddValidRtpReceiver(uint32_t rtp_sender_ssrc,
                           uint32_t rtp_receiver_ssrc) override {}
  void InitializeRtpReceiverRtcpBuilder(
      uint32_t rtp_receiver_ssrc,
      const media::cast::RtcpTimeData& time_data) override {}
  void AddCastFeedback(const media::cast::RtcpCastMessage& cast_message,
                       base::TimeDelta target_delay) override {}
  void AddPli(const media::cast::RtcpPliMessage& pli_message) override {}
  void AddRtcpEvents(
      const media::cast::ReceiverRtcpEventSubscriber::RtcpEvents& e) override {}
  void AddRtpReceiverReport(const media::cast::RtcpReportBlock& b) override {}
  void SendRtcpFromRtpReceiver() override {}
  void SetOptions(const base::Value::Dict& options) override {}

 private:
  std::vector<media::cast::EncodedFrame> sent_frames_;
  std::vector<media::cast::FrameId> canceled_frame_ids_;

  base::RepeatingClosure kickstarted_callback_;
  media::cast::FrameId kickstarted_frame_id_;
};

}  // namespace

class RemotingSenderTest : public ::testing::Test {
 public:
  RemotingSenderTest(const RemotingSenderTest&) = delete;
  RemotingSenderTest& operator=(const RemotingSenderTest&) = delete;

 protected:
  RemotingSenderTest()
      : cast_environment_(new media::cast::CastEnvironment(
            base::DefaultTickClock::GetInstance(),
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner())),
        expecting_error_callback_run_(false),
        receiver_ssrc_(-1) {
    const MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        kDataPipeCapacity};
    mojo::ScopedDataPipeConsumerHandle consumer_end;
    CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&data_pipe_options,
                                                  producer_end_, consumer_end));

    media::cast::FrameSenderConfig video_config =
        media::cast::GetDefaultVideoSenderConfig();
    video_config.rtp_payload_type = media::cast::RtpPayloadType::REMOTE_VIDEO;
    video_config.codec = media::cast::CODEC_VIDEO_REMOTE;
    receiver_ssrc_ = video_config.receiver_ssrc;
    remoting_sender_ = std::make_unique<RemotingSender>(
        cast_environment_, &transport_, video_config, std::move(consumer_end),
        sender_.BindNewPipeAndPassReceiver(),
        base::BindOnce(
            [](bool expecting_error_callback_run) {
              CHECK(expecting_error_callback_run);
            },
            expecting_error_callback_run_));

    // Give CastRemotingSender a small RTT measurement to prevent kickstart
    // testing from taking too long.
    remoting_sender_->frame_sender_->OnMeasuredRoundTripTime(
        base::Milliseconds(1));
    RunPendingTasks();
  }

  ~RemotingSenderTest() override {}

  void TearDown() final {
    remoting_sender_.reset();
    // Allow any pending tasks to run before destruction.
    RunPendingTasks();
  }

  // Allow pending tasks, such as Mojo method calls, to execute.
  void RunPendingTasks() { task_environment_.RunUntilIdle(); }

 protected:
  media::cast::FrameId last_acked_frame_id() const {
    return remoting_sender_->frame_sender_->LastAckedFrameId();
  }

  int NumberOfFramesInFlight() {
    return remoting_sender_->frame_sender_->GetUnacknowledgedFrameCount();
  }

  size_t GetSizeOfNextFrameData() {
    return remoting_sender_->next_frame_data_.size();
  }

  bool IsFlowRestartPending() const {
    return remoting_sender_->flow_restart_pending_;
  }

  [[nodiscard]] bool ProduceDataChunk(size_t offset, size_t size) {
    std::vector<uint8_t> fake_chunk(size);
    for (size_t i = 0; i < size; ++i)
      fake_chunk[i] = static_cast<uint8_t>(offset + i);
    uint32_t num_bytes = fake_chunk.size();
    return producer_end_->WriteData(fake_chunk.data(), &num_bytes,
                                    MOJO_WRITE_DATA_FLAG_ALL_OR_NONE) ==
           MOJO_RESULT_OK;
  }

  void SendFrame(uint32_t size) { remoting_sender_->SendFrame(size); }

  void CancelInFlightData() { remoting_sender_->CancelInFlightData(); }

  void TakeSentFrames(std::vector<media::cast::EncodedFrame>* frames) {
    transport_.TakeSentFrames(frames);
  }

  bool ExpectOneFrameWasSent(size_t expected_payload_size) {
    std::vector<media::cast::EncodedFrame> frames;
    transport_.TakeSentFrames(&frames);
    EXPECT_EQ(1u, frames.size());
    if (frames.empty())
      return false;
    return ExpectCorrectFrameData(expected_payload_size, frames.front());
  }

  void AckUpToAndIncluding(media::cast::FrameId frame_id) {
    media::cast::RtcpCastMessage cast_feedback(receiver_ssrc_);
    cast_feedback.ack_frame_id = frame_id;
    remoting_sender_->frame_sender_->OnReceivedCastFeedback(cast_feedback);
  }

  void AckOldestInFlightFrames(int count) {
    AckUpToAndIncluding(last_acked_frame_id() + count);
  }

  // Blocks the caller indefinitely, until a kickstart frame is sent, and then
  // returns the FrameId of the kickstarted-frame.
  media::cast::FrameId WaitForKickstart() {
    return transport_.WaitForKickstart();
  }

  bool ExpectNoFramesCanceled() {
    std::vector<media::cast::FrameId> frame_ids;
    transport_.TakeCanceledFrameIds(&frame_ids);
    return frame_ids.empty();
  }

  bool ExpectFramesCanceled(media::cast::FrameId first_frame_id,
                            media::cast::FrameId last_frame_id) {
    std::vector<media::cast::FrameId> frame_ids;
    transport_.TakeCanceledFrameIds(&frame_ids);
    auto begin = frame_ids.begin();
    auto end = frame_ids.end();
    for (auto fid = first_frame_id; fid <= last_frame_id; ++fid) {
      auto new_end = std::remove(begin, end, fid);
      if (new_end == end)
        return false;
      end = new_end;
    }
    return begin == end;
  }

  static bool ExpectCorrectFrameData(size_t expected_payload_size,
                                     const media::cast::EncodedFrame& frame) {
    if (expected_payload_size != frame.data.size()) {
      ADD_FAILURE() << "Expected frame data size != frame.data.size(): "
                    << expected_payload_size << " vs " << frame.data.size();
      return false;
    }
    for (size_t i = 0; i < expected_payload_size; ++i) {
      if (static_cast<uint8_t>(frame.data[i]) != static_cast<uint8_t>(i)) {
        ADD_FAILURE() << "Frame data byte mismatch at offset " << i;
        return false;
      }
    }
    return true;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  const scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  FakeTransport transport_;
  std::unique_ptr<RemotingSender> remoting_sender_;
  mojo::Remote<media::mojom::RemotingDataStreamSender> sender_;
  mojo::ScopedDataPipeProducerHandle producer_end_;
  bool expecting_error_callback_run_;
  uint32_t receiver_ssrc_;
};

TEST_F(RemotingSenderTest, SendsFramesViaMojoDataPipe) {
  // One 256-byte chunk pushed through the data pipe to make one frame.
  ASSERT_TRUE(ProduceDataChunk(0, 256));
  SendFrame(256);
  RunPendingTasks();
  EXPECT_TRUE(ExpectOneFrameWasSent(256));
  AckOldestInFlightFrames(1);
  EXPECT_EQ(media::cast::FrameId::first(), last_acked_frame_id());

  // Four 256-byte chunks pushed through the data pipe to make one frame.
  SendFrame(1024);
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(ProduceDataChunk(i * 256, 256));
  }
  RunPendingTasks();
  EXPECT_TRUE(ExpectOneFrameWasSent(1024));
  AckOldestInFlightFrames(1);
  EXPECT_EQ(media::cast::FrameId::first() + 1, last_acked_frame_id());

  // 10 differently-sized chunks pushed through the data pipe to make one frame
  // that is larger than the data pipe's total capacity.
  SendFrame(6665);
  size_t offset = 0;
  for (int i = 0; i < 10; ++i) {
    const size_t chunk_size = 500 + i * 37;
    ASSERT_TRUE(ProduceDataChunk(offset, chunk_size));
    RunPendingTasks();
    offset += chunk_size;
  }
  RunPendingTasks();
  EXPECT_TRUE(ExpectOneFrameWasSent(6665));
  AckOldestInFlightFrames(1);
  EXPECT_EQ(media::cast::FrameId::first() + 2, last_acked_frame_id());
}

TEST_F(RemotingSenderTest, SendsMultipleFramesWithDelayedAcks) {
  // Send 4 frames.
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(ProduceDataChunk(0, 16));
    SendFrame(16);
  }
  RunPendingTasks();
  EXPECT_EQ(4, NumberOfFramesInFlight());
  EXPECT_TRUE(ExpectNoFramesCanceled());

  // Ack one frame.
  AckOldestInFlightFrames(1);
  EXPECT_EQ(3, NumberOfFramesInFlight());
  EXPECT_TRUE(ExpectFramesCanceled(media::cast::FrameId::first(),
                                   media::cast::FrameId::first()));

  // Ack all.
  AckOldestInFlightFrames(3);
  EXPECT_EQ(0, NumberOfFramesInFlight());
  EXPECT_TRUE(ExpectFramesCanceled(media::cast::FrameId::first() + 1,
                                   media::cast::FrameId::first() + 3));
}

TEST_F(RemotingSenderTest, KickstartsIfAckNotTimely) {
  // Send first frame and don't Ack it. Expect the first frame to be
  // kickstarted.
  ASSERT_TRUE(ProduceDataChunk(0, 16));
  SendFrame(16);
  EXPECT_EQ(media::cast::FrameId::first(), WaitForKickstart());
  EXPECT_EQ(1, NumberOfFramesInFlight());

  // Send 3 more frames and don't Ack them either. Expect the 4th frame to be
  // kickstarted.
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(ProduceDataChunk(0, 16));
    SendFrame(16);
  }
  EXPECT_EQ(media::cast::FrameId::first() + 3, WaitForKickstart());
  EXPECT_EQ(4, NumberOfFramesInFlight());

  // Ack the first two frames and wait for another kickstart (for the 4th frame
  // again).
  AckOldestInFlightFrames(2);
  EXPECT_EQ(2, NumberOfFramesInFlight());
  EXPECT_EQ(media::cast::FrameId::first() + 3, WaitForKickstart());
}

TEST_F(RemotingSenderTest, CancelsUnsentFrame) {
  EXPECT_EQ(0u, GetSizeOfNextFrameData());
  SendFrame(16);
  SendFrame(32);
  CancelInFlightData();

  // Provide the data. Both frames should not be sent out.
  ASSERT_TRUE(ProduceDataChunk(0, 16));
  RunPendingTasks();
  ASSERT_TRUE(ProduceDataChunk(0, 32));
  RunPendingTasks();
  EXPECT_EQ(0, NumberOfFramesInFlight());

  // Since no frames were sent, none should have been passed to the
  // CastTransport, and none should have been canceled.
  std::vector<media::cast::EncodedFrame> frames;
  TakeSentFrames(&frames);
  EXPECT_TRUE(frames.empty());
  EXPECT_TRUE(ExpectNoFramesCanceled());
}

TEST_F(RemotingSenderTest, CancelsFramesInFlight) {
  EXPECT_TRUE(IsFlowRestartPending());

  // Send 10 frames.
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(ProduceDataChunk(0, 16));
    SendFrame(16);
  }
  RunPendingTasks();
  EXPECT_FALSE(IsFlowRestartPending());
  EXPECT_EQ(10, NumberOfFramesInFlight());

  // Ack the first frame.
  AckOldestInFlightFrames(1);
  EXPECT_FALSE(IsFlowRestartPending());
  EXPECT_EQ(9, NumberOfFramesInFlight());
  EXPECT_TRUE(ExpectFramesCanceled(media::cast::FrameId::first(),
                                   media::cast::FrameId::first()));

  // Despite the name, this does not actually cancel in-flight frames, as that
  // capability was never implemented.
  CancelInFlightData();
  RunPendingTasks();
  EXPECT_TRUE(IsFlowRestartPending());
  EXPECT_EQ(9, NumberOfFramesInFlight());

  // Send one more frame and ack it.
  ASSERT_TRUE(ProduceDataChunk(0, 16));
  SendFrame(16);
  RunPendingTasks();
  EXPECT_FALSE(IsFlowRestartPending());
  EXPECT_EQ(10, NumberOfFramesInFlight());
  AckOldestInFlightFrames(1);
  EXPECT_EQ(9, NumberOfFramesInFlight());

  // Check that the dependency metadata was set correctly to indicate a frame
  // that immediately follows a CancelInFlightData() operation.
  std::vector<media::cast::EncodedFrame> frames;
  TakeSentFrames(&frames);
  ASSERT_EQ(11u, frames.size());
  for (size_t i = 0; i < 11; ++i) {
    const media::cast::EncodedFrame& frame = frames[i];
    EXPECT_EQ(media::cast::FrameId::first() + i, frame.frame_id);
    if (i == 0 || i == 10)
      EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
    else
      EXPECT_EQ(Dependency::kDependent, frame.dependency);
  }
}

TEST_F(RemotingSenderTest, WaitsForDataBeforeConsumingFromDataPipe) {
  // Queue up and issue Mojo calls to consume three frames. Since no data has
  // been pushed into the pipe yet no frames should be sent.
  for (int i = 0; i < 3; ++i) {
    SendFrame(4);
  }
  RunPendingTasks();
  EXPECT_TRUE(IsFlowRestartPending());
  EXPECT_EQ(0, NumberOfFramesInFlight());

  // Push the data for one frame into the data pipe. This should trigger input
  // processing and allow one frame to be sent.
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  RunPendingTasks();  // Allow Mojo Watcher to signal CastRemotingSender.
  EXPECT_FALSE(IsFlowRestartPending());
  EXPECT_EQ(1, NumberOfFramesInFlight());

  // Now push the data for the other two frames into the data pipe and expect
  // two more frames to be sent.
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  RunPendingTasks();  // Allow Mojo Watcher to signal CastRemotingSender.
  EXPECT_FALSE(IsFlowRestartPending());
  EXPECT_EQ(3, NumberOfFramesInFlight());
}

TEST_F(RemotingSenderTest, WaitsForDataThenDiscardsCanceledData) {
  // Queue up and issue Mojo calls to consume data chunks and send three
  // frames. Since no data has been pushed into the pipe yet no frames should be
  // sent.
  for (int i = 0; i < 3; ++i) {
    SendFrame(4);
  }
  RunPendingTasks();
  EXPECT_EQ(0, NumberOfFramesInFlight());

  // Cancel all in-flight data.
  CancelInFlightData();
  RunPendingTasks();

  // Now, push the data for one frame into the data pipe. Because of the
  // cancellation, no frames should be sent.
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  RunPendingTasks();  // Allow Mojo Watcher to signal CastRemotingSender.
  EXPECT_EQ(0, NumberOfFramesInFlight());

  // Now push the data for the other two frames into the data pipe and still no
  // frames should be sent.
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  RunPendingTasks();  // Allow Mojo Watcher to signal CastRemotingSender.
  EXPECT_EQ(0, NumberOfFramesInFlight());

  // Now issue calls to send another frame and then push the data for it into
  // the data pipe. Expect to see the frame gets sent since it was provided
  // after the CancelInFlightData().
  SendFrame(4);
  RunPendingTasks();
  EXPECT_EQ(0, NumberOfFramesInFlight());
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  RunPendingTasks();  // Allow Mojo Watcher to signal CastRemotingSender.
  EXPECT_EQ(1, NumberOfFramesInFlight());
}

TEST_F(RemotingSenderTest, StopsConsumingWhileTooManyFramesAreInFlight) {
  EXPECT_TRUE(IsFlowRestartPending());

  // Send out the maximum possible number of unacked frames, but don't ack any
  // yet.
  for (int i = 0; i < media::cast::kMaxUnackedFrames; ++i) {
    ASSERT_TRUE(ProduceDataChunk(0, 4));
    SendFrame(4);
  }
  RunPendingTasks();
  EXPECT_FALSE(IsFlowRestartPending());
  EXPECT_EQ(media::cast::kMaxUnackedFrames, NumberOfFramesInFlight());
  // Note: All frames should have been sent to the Transport, and so
  // CastRemotingSender's single-frame data buffer should be empty.
  EXPECT_EQ(0u, GetSizeOfNextFrameData());

  // When the client provides one more frame, CastRemotingSender will begin
  // queuing input operations instead of sending the the frame to the
  // CastTransport.
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  SendFrame(4);
  RunPendingTasks();
  EXPECT_EQ(media::cast::kMaxUnackedFrames, NumberOfFramesInFlight());
  // Note: The unsent frame resides in CastRemotingSender's single-frame data
  // buffer.
  EXPECT_EQ(4u, GetSizeOfNextFrameData());

  // Ack the the first frame and expect sending to resume, with one more frame
  // being sent to the CastTransport.
  AckOldestInFlightFrames(1);
  EXPECT_EQ(media::cast::kMaxUnackedFrames, NumberOfFramesInFlight());
  // Note: Only one frame was backlogged, and so CastRemotingSender's
  // single-frame data buffer should be empty.
  EXPECT_EQ(0u, GetSizeOfNextFrameData());

  // Attempting to send another frame will once again cause CastRemotingSender
  // to queue input operations.
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  SendFrame(4);
  RunPendingTasks();
  EXPECT_EQ(media::cast::kMaxUnackedFrames, NumberOfFramesInFlight());
  // Note: Once again, CastRemotingSender's single-frame data buffer contains an
  // unsent frame.
  EXPECT_EQ(4u, GetSizeOfNextFrameData());

  // Send more frames: Some number of frames will queue-up inside the Mojo data
  // pipe (the exact number depends on the data pipe's capacity, and how Mojo
  // manages memory internally). At some point, attempting to produce and push
  // another frame will fail because the data pipe is full.
  int num_frames_in_data_pipe = 0;
  while (ProduceDataChunk(0, 768)) {
    ++num_frames_in_data_pipe;
    SendFrame(768);
    RunPendingTasks();
    EXPECT_EQ(media::cast::kMaxUnackedFrames, NumberOfFramesInFlight());
    // Note: CastRemotingSender's single-frame data buffer should still contain
    // the unsent 4-byte frame.
    EXPECT_EQ(4u, GetSizeOfNextFrameData());
  }
  EXPECT_LT(0, num_frames_in_data_pipe);

  // Ack one frame at a time until the backlog in the Mojo data pipe has
  // cleared.
  int remaining_frames_in_data_pipe = num_frames_in_data_pipe;
  while (remaining_frames_in_data_pipe > 0) {
    AckOldestInFlightFrames(1);
    RunPendingTasks();
    --remaining_frames_in_data_pipe;
    EXPECT_EQ(media::cast::kMaxUnackedFrames, NumberOfFramesInFlight());
    EXPECT_EQ(768u, GetSizeOfNextFrameData());
  }

  // Ack one more frame. There should no longer be a backlog on the input side
  // of things.
  AckOldestInFlightFrames(1);
  RunPendingTasks();  // No additional Mojo method calls should be made here.
  EXPECT_EQ(media::cast::kMaxUnackedFrames, NumberOfFramesInFlight());
  // The single-frame data buffer should be empty to indicate no input backlog.
  EXPECT_EQ(0u, GetSizeOfNextFrameData());

  // Ack all but one frame.
  AckOldestInFlightFrames(NumberOfFramesInFlight() - 1);
  EXPECT_EQ(1, NumberOfFramesInFlight());
  // ..and one more frame can be sent immediately.
  ASSERT_TRUE(ProduceDataChunk(0, 4));
  SendFrame(4);
  RunPendingTasks();
  EXPECT_EQ(2, NumberOfFramesInFlight());
  // ...and ack these last two frames.
  AckOldestInFlightFrames(2);
  EXPECT_EQ(0, NumberOfFramesInFlight());

  // Finally, examine all frames that were sent to the CastTransport, and
  // confirm their metadata and data is valid.
  std::vector<media::cast::EncodedFrame> frames;
  TakeSentFrames(&frames);
  const size_t total_frames_sent =
      media::cast::kMaxUnackedFrames + 2 + num_frames_in_data_pipe + 1;
  ASSERT_EQ(total_frames_sent, frames.size());
  media::cast::RtpTimeTicks last_rtp_timestamp =
      media::cast::RtpTimeTicks() - media::cast::RtpTimeDelta::FromTicks(1);
  for (size_t i = 0; i < total_frames_sent; ++i) {
    const media::cast::EncodedFrame& frame = frames[i];
    EXPECT_EQ(media::cast::FrameId::first() + i, frame.frame_id);
    if (i == 0) {
      EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
      EXPECT_EQ(media::cast::FrameId::first() + i, frame.referenced_frame_id);
    } else {
      EXPECT_EQ(Dependency::kDependent, frame.dependency);
      EXPECT_EQ(media::cast::FrameId::first() + i - 1,
                frame.referenced_frame_id);
    }

    // RTP timestamp must be monotonically increasing.
    EXPECT_GT(frame.rtp_timestamp, last_rtp_timestamp);
    last_rtp_timestamp = frame.rtp_timestamp;

    size_t expected_frame_size = 4;
    if ((i >= media::cast::kMaxUnackedFrames + 2u) &&
        (i < media::cast::kMaxUnackedFrames + 2u + num_frames_in_data_pipe)) {
      expected_frame_size = 768;
    }
    EXPECT_TRUE(ExpectCorrectFrameData(expected_frame_size, frame));
  }
}

}  // namespace mirroring
