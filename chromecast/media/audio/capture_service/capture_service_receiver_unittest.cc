// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/capture_service_receiver.h"

#include <memory>

#include "base/big_endian.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "chromecast/media/audio/mock_audio_input_callback.h"
#include "chromecast/net/mock_stream_socket.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromecast {
namespace media {
namespace capture_service {
namespace {

class MockStreamSocket : public chromecast::MockStreamSocket {
 public:
  MockStreamSocket() = default;
  ~MockStreamSocket() override = default;
};

class CaptureServiceReceiverTest : public ::testing::Test {
 public:
  CaptureServiceReceiverTest()
      : receiver_(::media::AudioParameters(
            ::media::AudioParameters::AUDIO_PCM_LINEAR,
            ::media::ChannelLayout::CHANNEL_LAYOUT_MONO,
            16000,
            160)) {
    receiver_.SetTaskRunnerForTest(base::CreateSequencedTaskRunner(
        {base::ThreadPool(), base::TaskPriority::USER_BLOCKING}));
  }
  ~CaptureServiceReceiverTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  chromecast::MockAudioInputCallback audio_;
  CaptureServiceReceiver receiver_;
};

TEST_F(CaptureServiceReceiverTest, StartStop) {
  auto socket1 = std::make_unique<MockStreamSocket>();
  auto socket2 = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket1, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket1, Read(_, _, _)).WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(*socket2, Connect(_)).WillOnce(Return(net::OK));

  // Sync.
  receiver_.StartWithSocket(&audio_, std::move(socket1));
  task_environment_.RunUntilIdle();
  receiver_.Stop();

  // Async.
  receiver_.StartWithSocket(&audio_, std::move(socket2));
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ConnectFailed) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::ERR_FAILED));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ConnectTimeout) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.FastForwardBy(CaptureServiceReceiver::kConnectTimeout);
}

TEST_F(CaptureServiceReceiverTest, ReceiveValidMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Invoke([](net::IOBuffer* buf, int,
                          net::CompletionOnceCallback) {
        std::vector<char> header(16);
        base::BigEndianWriter data_writer(header.data(), header.size());
        data_writer.WriteU16(334);  // 160 frames + header - data[0], in bytes.
        data_writer.WriteU16(1);    // Mono channel.
        data_writer.WriteU16(0);    // Interleaved int16.
        data_writer.WriteU16(0);    // Padding zero.
        data_writer.WriteU64(0);    // Timestamp.
        std::copy(header.data(), header.data() + header.size(), buf->data());
        // No need to fill audio frames.
        return 336;  // 160 frames + header, in bytes.
      }))
      .WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(audio_, OnData(_, _, 1.0 /* volume */));

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
  // Stop receiver to disconnect socket, since receiver doesn't own the IO
  // task runner in unittests.
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveEmptyMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Invoke([](net::IOBuffer* buf, int,
                          net::CompletionOnceCallback) {
        std::vector<char> header(16, 0);
        base::BigEndianWriter data_writer(header.data(), header.size());
        data_writer.WriteU16(14);  // header - data[0], in bytes.
        std::copy(header.data(), header.data() + header.size(), buf->data());
        return 16;
      }));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveInvalidMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Invoke([](net::IOBuffer* buf, int,
                          net::CompletionOnceCallback) {
        std::vector<char> header(16, 0);
        base::BigEndianWriter data_writer(header.data(), header.size());
        data_writer.WriteU16(334);  // 160 frames + header - data[0], in bytes.
        data_writer.WriteU16(1);    // Mono channels.
        data_writer.WriteU16(6);    // Invalid format.
        data_writer.WriteU16(0);    // Padding zero.
        data_writer.WriteU64(0);    // Timestamp.
        std::copy(header.data(), header.data() + header.size(), buf->data());
        // No need to fill audio frames.
        return 336;
      }));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveError) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Return(net::ERR_CONNECTION_RESET));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveEosMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Read(_, _, _)).WillOnce(Return(0));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
