// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/frame/stream_consumer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/tracing/test_trace_processor.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/cast_streaming/browser/common/decoder_buffer_factory.h"
#include "components/cast_streaming/common/frame/frame_trace_flow.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/impl/receiver_packet_router.h"  // nogncheck
#include "third_party/openscreen/src/cast/streaming/public/constants.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/frame_trace_ids.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver.h"

namespace cast_streaming {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Return;

openscreen::Clock::time_point FakeNow() {
  return openscreen::Clock::time_point(
      std::chrono::duration_cast<openscreen::Clock::duration>(
          std::chrono::steady_clock::now().time_since_epoch()));
}

class TaskRunnerAdapter : public openscreen::TaskRunner {
 public:
  explicit TaskRunnerAdapter(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  ~TaskRunnerAdapter() override = default;

  void PostPackagedTask(Task task) override {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce([](Task t) { t(); }, std::move(task)));
  }

  void PostPackagedTaskWithDelay(Task task,
                                 openscreen::Clock::duration delay) override {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce([](Task t) { t(); }, std::move(task)),
        base::Microseconds(
            std::chrono::duration_cast<std::chrono::microseconds>(delay)
                .count()));
  }

  bool IsRunningOnTaskRunner() override {
    return task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

class MockReceiver : public openscreen::cast::Receiver {
 public:
  explicit MockReceiver(openscreen::cast::SessionConfig config)
      : config_(std::move(config)) {
    ON_CALL(*this, config).WillByDefault(testing::ReturnRef(config_));
  }

  MOCK_METHOD(const openscreen::cast::SessionConfig&,
              config,
              (),
              (const, override));
  MOCK_METHOD(void, SetConsumer, (Consumer*), (override));
  MOCK_METHOD(void,
              SetPlayerProcessingTime,
              (openscreen::Clock::duration),
              (override));
  MOCK_METHOD(openscreen::Error,
              ReportPlayoutEvent,
              (openscreen::cast::FrameId,
               openscreen::cast::RtpTimeTicks,
               openscreen::Clock::time_point),
              (override));
  MOCK_METHOD(void, RequestKeyFrame, (), (override));
  MOCK_METHOD(std::optional<size_t>, AdvanceToNextFrame, (), (override));
  MOCK_METHOD(openscreen::cast::EncodedFrame,
              ConsumeNextFrame,
              (openscreen::ByteBuffer),
              (override));

 private:
  openscreen::cast::SessionConfig config_;
};

class MockDecoderBufferFactory : public DecoderBufferFactory {
 public:
  MOCK_METHOD(scoped_refptr<media::DecoderBuffer>,
              ToDecoderBuffer,
              (const openscreen::cast::EncodedFrame&,
               base::span<const uint8_t>),
              (override));
};

class StreamConsumerTest : public testing::Test {
 public:
  StreamConsumerTest()
      : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        environment_(&FakeNow, task_runner_, openscreen::IPEndpoint::kAnyV4()),
        packet_router_(environment_),
        receiver_(openscreen::cast::SessionConfig(
            1 /* sender_ssrc */,
            2 /* receiver_ssrc */,
            90000 /* rtp_timebase */,
            2 /* channels */,
            std::chrono::milliseconds(100) /* target_playout_delay */,
            std::array<uint8_t, 16>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                    14, 15, 16} /* aes_secret_key */,
            std::array<uint8_t, 16>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                    14, 15, 16} /* aes_iv_mask */)) {
    mojo::CreateDataPipe(1024 * 1024, producer_handle_, consumer_handle_);
  }

  void SetUp() override {
    EXPECT_CALL(receiver_, SetConsumer(_));

    auto decoder_buffer_factory = std::make_unique<MockDecoderBufferFactory>();
    decoder_buffer_factory_ = decoder_buffer_factory.get();
    consumer_ = std::make_unique<StreamConsumer>(
        &receiver_, media::DemuxerStream::Type::VIDEO,
        std::move(producer_handle_),
        base::BindRepeating(&StreamConsumerTest::OnFrameReceived,
                            base::Unretained(this)),
        base::BindRepeating(&StreamConsumerTest::OnNewFrame,
                            base::Unretained(this)),
        std::move(decoder_buffer_factory));
  }

  void OnFrameReceived(media::mojom::DecoderBufferPtr buffer) {}
  void OnNewFrame() {}

 protected:
  base::test::TaskEnvironment task_environment_;
  TaskRunnerAdapter task_runner_;
  openscreen::cast::Environment environment_;
  openscreen::cast::ReceiverPacketRouter packet_router_;
  testing::NiceMock<MockReceiver> receiver_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  std::unique_ptr<StreamConsumer> consumer_;
  raw_ptr<MockDecoderBufferFactory> decoder_buffer_factory_ = nullptr;
};

TEST_F(StreamConsumerTest, LargeFrame) {
  // Current limit is 10 * 1024 * 1024.
  // We simulate a frame size of 530000 (which previously failed).
  size_t frame_size = 530000;

  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(frame_size))
      .WillRepeatedly(Return(std::nullopt));

  openscreen::cast::EncodedFrame frame;
  frame.frame_id = openscreen::cast::FrameId::first() + 1;
  EXPECT_CALL(receiver_, ConsumeNextFrame(_))
      .WillOnce(Return(std::move(frame)))
      .WillRepeatedly([](openscreen::ByteBuffer) {
        return openscreen::cast::EncodedFrame();
      });
  EXPECT_CALL(*decoder_buffer_factory_, ToDecoderBuffer(_, _))
      .WillOnce(Return(base::MakeRefCounted<media::DecoderBuffer>(0)));

  // Trigger read.
  consumer_->ReadFrame(base::DoNothing());

  // Check if pipe is NOT closed.
  size_t num_bytes = 0;
  MojoResult result = consumer_handle_->ReadData(
      MOJO_READ_DATA_FLAG_NONE, base::span<uint8_t>(), num_bytes);
  EXPECT_NE(result, MOJO_RESULT_FAILED_PRECONDITION);
}

TEST_F(StreamConsumerTest, ReceiverFrameContentsCoverage) {
  size_t frame_size = 100;

  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(frame_size))
      .WillRepeatedly(Return(std::nullopt));

  openscreen::cast::EncodedFrame frame;
  frame.frame_id = openscreen::cast::FrameId::first() + 1;
  EXPECT_CALL(receiver_, ConsumeNextFrame(_))
      .WillOnce(Return(std::move(frame)))
      .WillRepeatedly([](openscreen::ByteBuffer) {
        return openscreen::cast::EncodedFrame();
      });

  EXPECT_CALL(*decoder_buffer_factory_, ToDecoderBuffer(_, _))
      .WillOnce([](const openscreen::cast::EncodedFrame& encoded_frame,
                   base::span<const uint8_t> frame_data) {
        // Verify that the frame content size matches exactly what we simulated.
        EXPECT_EQ(frame_data.size(), 100u);

        return base::MakeRefCounted<media::DecoderBuffer>(0);
      });

  // Trigger read.
  consumer_->ReadFrame(base::DoNothing());
}

TEST_F(StreamConsumerTest, FrameReallyTooBig) {
  // Limit is 10MB. Simulate 11MB.
  size_t frame_size = 11 * 1024 * 1024;

  EXPECT_CALL(receiver_, AdvanceToNextFrame()).WillOnce(Return(frame_size));
  EXPECT_CALL(receiver_, ConsumeNextFrame(_)).Times(0);

  // Trigger read.
  consumer_->ReadFrame(base::DoNothing());

  // Check if pipe is closed.
  size_t num_bytes = 0;
  MojoResult result = consumer_handle_->ReadData(
      MOJO_READ_DATA_FLAG_NONE, base::span<uint8_t>(), num_bytes);
  EXPECT_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
}

TEST_F(StreamConsumerTest, ReportsPlayoutEventOnFrameRead) {
  const size_t frame_size = 100;
  const openscreen::cast::FrameId expected_frame_id =
      openscreen::cast::FrameId::first() + 1;
  const openscreen::cast::RtpTimeTicks expected_rtp_timestamp =
      openscreen::cast::RtpTimeTicks() +
      openscreen::cast::RtpTimeDelta::FromTicks(9000);

  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(frame_size))
      .WillRepeatedly(Return(std::nullopt));

  openscreen::cast::EncodedFrame frame;
  frame.frame_id = expected_frame_id;
  frame.rtp_timestamp = expected_rtp_timestamp;
  EXPECT_CALL(receiver_, ConsumeNextFrame(_))
      .WillOnce(Return(std::move(frame)));
  EXPECT_CALL(*decoder_buffer_factory_, ToDecoderBuffer(_, _))
      .WillOnce(Return(base::MakeRefCounted<media::DecoderBuffer>(0)));
  EXPECT_CALL(receiver_,
              ReportPlayoutEvent(expected_frame_id, expected_rtp_timestamp, _))
      .WillOnce(Return(openscreen::Error::None()));

  consumer_->ReadFrame(base::DoNothing());
}

TEST_F(StreamConsumerTest, DoesNotReportPlayoutEventForSkippedFrames) {
  const size_t frame_size = 100;
  const openscreen::cast::FrameId skipped_frame_id =
      openscreen::cast::FrameId::first();
  const openscreen::cast::FrameId kept_frame_id =
      openscreen::cast::FrameId::first() + 2;
  const openscreen::cast::RtpTimeTicks kept_rtp_timestamp =
      openscreen::cast::RtpTimeTicks() +
      openscreen::cast::RtpTimeDelta::FromTicks(18000);

  consumer_->FlushUntil(2);

  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(frame_size))
      .WillOnce(Return(frame_size))
      .WillRepeatedly(Return(std::nullopt));

  openscreen::cast::EncodedFrame skipped_frame;
  skipped_frame.frame_id = skipped_frame_id;
  openscreen::cast::EncodedFrame kept_frame;
  kept_frame.frame_id = kept_frame_id;
  kept_frame.rtp_timestamp = kept_rtp_timestamp;

  EXPECT_CALL(receiver_, ConsumeNextFrame(_))
      .WillOnce(Return(std::move(skipped_frame)))
      .WillOnce(Return(std::move(kept_frame)));
  EXPECT_CALL(*decoder_buffer_factory_, ToDecoderBuffer(_, _))
      .WillOnce(Return(base::MakeRefCounted<media::DecoderBuffer>(0)));
  EXPECT_CALL(receiver_, ReportPlayoutEvent(skipped_frame_id, _, _)).Times(0);
  EXPECT_CALL(receiver_,
              ReportPlayoutEvent(kept_frame_id, kept_rtp_timestamp, _))
      .WillOnce(Return(openscreen::Error::None()));

  consumer_->ReadFrame(base::DoNothing());
}

TEST_F(StreamConsumerTest,
       DoesNotReportPlayoutEventWhenDecoderBufferCreationFails) {
  const size_t frame_size = 100;

  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(frame_size))
      .WillRepeatedly(Return(std::nullopt));

  openscreen::cast::EncodedFrame frame;
  frame.frame_id = openscreen::cast::FrameId::first();
  EXPECT_CALL(receiver_, ConsumeNextFrame(_))
      .WillOnce(Return(std::move(frame)));
  EXPECT_CALL(*decoder_buffer_factory_, ToDecoderBuffer(_, _))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(receiver_, ReportPlayoutEvent(_, _, _)).Times(0);

  consumer_->ReadFrame(base::DoNothing());
}

class StreamConsumerTracingTest : public StreamConsumerTest {
 protected:
  using Row = std::vector<std::string>;

  void StartTrace() { trace_processor_.StartTrace("cast.stream"); }

  base::test::TestTraceProcessor::QueryResult StopAndQuery(
      const std::string& query) {
    const absl::Status status = trace_processor_.StopAndParseTrace();
    EXPECT_TRUE(status.ok()) << status.message();
    auto result = trace_processor_.RunQuery(query);
    EXPECT_TRUE(result.has_value()) << result.error();
    return result.value_or(base::test::TestTraceProcessor::QueryResult());
  }

 private:
  base::test::TracingEnvironment tracing_environment_;
  base::test::TestTraceProcessor trace_processor_;
};

TEST_F(StreamConsumerTracingTest,
       TracesSkippedDroppedAndSentFramesWithFlowLinks) {
  StartTrace();

  const openscreen::cast::FrameId skipped_id =
      openscreen::cast::FrameId::first() + 1;
  const openscreen::cast::FrameId dropped_id =
      openscreen::cast::FrameId::first() + 2;
  const openscreen::cast::FrameId sent_id =
      openscreen::cast::FrameId::first() + 3;

  for (const auto id : {skipped_id, dropped_id, sent_id}) {
    TRACE_EVENT_INSTANT(
        "cast.stream", "Frame.Consumed", base::TimeTicks::Now(),
        perfetto::Flow::Global(openscreen::cast::GetFrameIdFlowId(
            receiver_.config().sender_ssrc, id)));
  }

  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(10))
      .WillOnce(Return(10))
      .WillOnce(Return(10))
      .WillRepeatedly(Return(std::nullopt));

  openscreen::cast::EncodedFrame skipped_frame;
  skipped_frame.frame_id = skipped_id;
  openscreen::cast::EncodedFrame dropped_frame;
  dropped_frame.frame_id = dropped_id;
  openscreen::cast::EncodedFrame sent_frame;
  sent_frame.frame_id = sent_id;
  EXPECT_CALL(receiver_, ConsumeNextFrame(_))
      .WillOnce(Return(std::move(skipped_frame)))
      .WillOnce(Return(std::move(dropped_frame)))
      .WillOnce(Return(std::move(sent_frame)));

  auto buffer = base::MakeRefCounted<media::DecoderBuffer>(4);
  buffer->set_timestamp(base::Milliseconds(33));
  EXPECT_CALL(*decoder_buffer_factory_, ToDecoderBuffer(_, _))
      .WillOnce(Return(nullptr))
      .WillOnce(Return(buffer));
  EXPECT_CALL(receiver_, ReportPlayoutEvent(sent_id, _, _))
      .WillOnce([&](openscreen::cast::FrameId, openscreen::cast::RtpTimeTicks,
                    openscreen::Clock::time_point) {
        TRACE_EVENT_INSTANT("cast.stream", "Frame.PlayedOut",
                            base::TimeTicks::Now(),
                            perfetto::TerminatingFlow::Global(
                                openscreen::cast::GetFrameIdFlowId(
                                    receiver_.config().sender_ssrc, sent_id)));
        return openscreen::Error::None();
      });

  consumer_->FlushUntil(static_cast<uint32_t>(dropped_id.value()));
  consumer_->ReadFrame(base::DoNothing());

  TRACE_EVENT_INSTANT(
      "cast.stream", "CastStreaming.Frame.Demuxed", base::TimeTicks::Now(),
      perfetto::TerminatingFlow::Global(GetRendererHandoffFlowId(
          media::DemuxerStream::Type::VIDEO, base::Milliseconds(33))));

  EXPECT_THAT(
      StopAndQuery(R"(
        SELECT
          slice_out.name AS from_name,
          slice_in.name AS to_name,
          EXTRACT_ARG(slice_in.arg_set_id, 'debug.frame_id') AS in_frame_id,
          EXTRACT_ARG(
              slice_in.arg_set_id, 'debug.skip_until_frame_id') AS skip_until
        FROM flow
        JOIN slice slice_out ON flow.slice_out = slice_out.id
        JOIN slice slice_in ON flow.slice_in = slice_in.id
        ORDER BY slice_out.id, slice_in.id
      )"),
      ElementsAre(
          Row{"from_name", "to_name", "in_frame_id", "skip_until"},
          Row{"Frame.Consumed", "CastStreaming.Frame.Skipped", "1", "2"},
          Row{"Frame.Consumed", "CastStreaming.Frame.Dropped", "2", "[NULL]"},
          Row{"Frame.Consumed", "CastStreaming.Frame.SentToRenderer", "3",
              "[NULL]"},
          Row{"CastStreaming.Frame.SentToRenderer", "Frame.PlayedOut", "[NULL]",
              "[NULL]"},
          Row{"CastStreaming.Frame.SentToRenderer",
              "CastStreaming.Frame.Demuxed", "[NULL]", "[NULL]"}));
}

TEST_F(StreamConsumerTracingTest, HandlesEndOfStreamBufferWithoutHandoffFlow) {
  StartTrace();

  const openscreen::cast::FrameId eos_id =
      openscreen::cast::FrameId::first() + 1;
  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(1))
      .WillRepeatedly(Return(std::nullopt));

  openscreen::cast::EncodedFrame eos_frame;
  eos_frame.frame_id = eos_id;
  EXPECT_CALL(receiver_, ConsumeNextFrame(_))
      .WillOnce(Return(std::move(eos_frame)));
  EXPECT_CALL(*decoder_buffer_factory_, ToDecoderBuffer(_, _))
      .WillOnce(Return(media::DecoderBuffer::CreateEOSBuffer()));

  consumer_->ReadFrame(base::DoNothing());

  EXPECT_THAT(
      StopAndQuery(R"(
        SELECT
          name,
          EXTRACT_ARG(arg_set_id, 'debug.end_of_stream') AS is_eos,
          EXTRACT_ARG(arg_set_id, 'debug.timestamp_us') AS timestamp_us
        FROM slice
      )"),
      ElementsAre(Row{"name", "is_eos", "timestamp_us"},
                  Row{"CastStreaming.Frame.SentToRenderer", "1", "[NULL]"}));
}

TEST_F(StreamConsumerTracingTest, SkipsSentEventWhenPipeWriteFails) {
  StartTrace();
  consumer_handle_.reset();

  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(10))
      .WillRepeatedly(Return(std::nullopt));

  openscreen::cast::EncodedFrame frame;
  frame.frame_id = openscreen::cast::FrameId::first() + 1;
  EXPECT_CALL(receiver_, ConsumeNextFrame(_))
      .WillOnce(Return(std::move(frame)));

  auto buffer = base::MakeRefCounted<media::DecoderBuffer>(4);
  buffer->set_timestamp(base::Milliseconds(33));
  EXPECT_CALL(*decoder_buffer_factory_, ToDecoderBuffer(_, _))
      .WillOnce(Return(buffer));

  consumer_->ReadFrame(base::DoNothing());

  EXPECT_THAT(StopAndQuery("SELECT name FROM slice"), ElementsAre(Row{"name"}));
}

}  // namespace
}  // namespace cast_streaming
