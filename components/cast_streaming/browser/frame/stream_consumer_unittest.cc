// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/frame/stream_consumer.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/cast_streaming/browser/common/decoder_buffer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/impl/receiver_packet_router.h"  // nogncheck
#include "third_party/openscreen/src/cast/streaming/public/constants.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver.h"

namespace cast_streaming {
namespace {

using testing::_;
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
  MockReceiver(openscreen::cast::Environment& environment,
               openscreen::cast::ReceiverPacketRouter& packet_router,
               openscreen::cast::SessionConfig config)
      : openscreen::cast::Receiver(environment, packet_router, config) {}

  MOCK_METHOD(int, AdvanceToNextFrame, (), (override));
  MOCK_METHOD(openscreen::cast::EncodedFrame,
              ConsumeNextFrame,
              (openscreen::ByteBuffer),
              (override));
  MOCK_METHOD(void, SetConsumer, (Consumer*), (override));
  MOCK_METHOD(openscreen::cast::Ssrc, ssrc, (), (const, override));
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
        receiver_(
            environment_,
            packet_router_,
            openscreen::cast::SessionConfig(
                1 /* sender_ssrc */,
                2 /* receiver_ssrc */,
                90000 /* rtp_timebase */,
                2 /* channels */,
                std::chrono::milliseconds(100) /* target_playout_delay */,
                std::array<uint8_t, 16>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                        13, 14, 15, 16} /* aes_secret_key */,
                std::array<uint8_t, 16>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                        13, 14, 15, 16} /* aes_iv_mask */)) {
    mojo::CreateDataPipe(1024 * 1024, producer_handle_, consumer_handle_);
  }

  void SetUp() override {
    EXPECT_CALL(receiver_, SetConsumer(_));
    EXPECT_CALL(receiver_, ssrc()).WillRepeatedly(Return(2));

    auto decoder_buffer_factory = std::make_unique<MockDecoderBufferFactory>();
    decoder_buffer_factory_ = decoder_buffer_factory.get();
    consumer_ = std::make_unique<StreamConsumer>(
        &receiver_, std::move(producer_handle_),
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
  int frame_size = 530000;

  EXPECT_CALL(receiver_, AdvanceToNextFrame())
      .WillOnce(Return(frame_size))
      .WillRepeatedly(Return(-1));

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
      .WillRepeatedly(Return(-1));

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
  int frame_size = 11 * 1024 * 1024;

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

}  // namespace
}  // namespace cast_streaming
