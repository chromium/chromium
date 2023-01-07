// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/capture_service_receiver.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "base/big_endian.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "chromecast/media/audio/capture_service/message_parsing_utils.h"
#include "chromecast/media/audio/capture_service/packet_header.h"
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

constexpr StreamInfo kStreamInfo =
    StreamInfo{.stream_type = StreamType::kSoftwareEchoCancelled,
               .audio_codec = AudioCodec::kPcm,
               .num_channels = 1,
               .sample_format = SampleFormat::PLANAR_FLOAT,
               .sample_rate = 16000,
               .frames_per_buffer = 160};
constexpr HandshakePacket kHandshakePacket = HandshakePacket{
    .size = 0,  // dummy
    .message_type = static_cast<uint8_t>(MessageType::kHandshake),
    .stream_type = static_cast<uint8_t>(kStreamInfo.stream_type),
    .audio_codec = static_cast<uint8_t>(kStreamInfo.audio_codec),
    .sample_format = static_cast<uint8_t>(kStreamInfo.sample_format),
    .num_channels = kStreamInfo.num_channels,
    .num_frames = kStreamInfo.frames_per_buffer,
    .sample_rate = kStreamInfo.sample_rate};
constexpr PcmPacketHeader kPcmAudioPacketHeader = PcmPacketHeader{
    .size = 0,  // dummy
    .message_type = static_cast<uint8_t>(MessageType::kPcmAudio),
    .stream_type = static_cast<uint8_t>(kStreamInfo.stream_type),
    .timestamp_us = 0};

class MockStreamSocket : public chromecast::MockStreamSocket {
 public:
  MockStreamSocket() = default;
  ~MockStreamSocket() override = default;
};

class MockCaptureServiceReceiverDelegate
    : public chromecast::media::CaptureServiceReceiver::Delegate {
 public:
  MockCaptureServiceReceiverDelegate() = default;
  ~MockCaptureServiceReceiverDelegate() override = default;

  MOCK_METHOD(bool, OnInitialStreamInfo, (const StreamInfo&), (override));
  MOCK_METHOD(bool, OnCaptureData, (const char*, size_t), (override));
  MOCK_METHOD(void, OnCaptureError, (), (override));
  MOCK_METHOD(void, OnCaptureMetadata, (const char*, size_t), (override));
};

class CaptureServiceReceiverTest : public ::testing::Test {
 public:
  CaptureServiceReceiverTest() : receiver_(kStreamInfo, &delegate_) {
    receiver_.SetTaskRunnerForTest(base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_BLOCKING}));
  }
  ~CaptureServiceReceiverTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockCaptureServiceReceiverDelegate delegate_;
  CaptureServiceReceiver receiver_;
};

TEST_F(CaptureServiceReceiverTest, StartStop) {
  auto socket1 = std::make_unique<MockStreamSocket>();
  auto socket2 = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket1, Connect).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket1, Write).WillOnce(Return(sizeof(HandshakePacket)));
  EXPECT_CALL(*socket1, Read).WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(*socket2, Connect).WillOnce(Return(net::OK));

  // Sync.
  receiver_.StartWithSocket(std::move(socket1));
  task_environment_.RunUntilIdle();
  receiver_.Stop();

  // Async.
  receiver_.StartWithSocket(std::move(socket2));
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ConnectFailed) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect).WillOnce(Return(net::ERR_FAILED));
  EXPECT_CALL(delegate_, OnCaptureError);

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ConnectTimeout) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect).WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(delegate_, OnCaptureError);

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.FastForwardBy(CaptureServiceReceiver::kConnectTimeout);
}

TEST_F(CaptureServiceReceiverTest, SendRequest) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write)
      .WillOnce(Invoke([](net::IOBuffer* buf, int buf_len,
                          net::CompletionOnceCallback,
                          const net::NetworkTrafficAnnotationTag&) {
        EXPECT_EQ(buf_len, static_cast<int>(sizeof(HandshakePacket)));
        auto* data = reinterpret_cast<const uint8_t*>(buf->data());
        uint16_t size;
        base::ReadBigEndian(data, &size);
        EXPECT_EQ(size, sizeof(HandshakePacket) - sizeof(size));
        HandshakePacket packet;
        std::memcpy(&packet, data, sizeof(HandshakePacket));
        EXPECT_EQ(packet.message_type, kHandshakePacket.message_type);
        EXPECT_EQ(packet.stream_type, kHandshakePacket.stream_type);
        EXPECT_EQ(packet.audio_codec, kHandshakePacket.audio_codec);
        EXPECT_EQ(packet.num_channels, kHandshakePacket.num_channels);
        EXPECT_EQ(packet.num_frames, kHandshakePacket.num_frames);
        EXPECT_EQ(packet.sample_rate, kHandshakePacket.sample_rate);
        return buf_len;
      }));
  EXPECT_CALL(*socket, Read).WillOnce(Return(net::ERR_IO_PENDING));

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
  // Stop receiver to disconnect socket, since receiver doesn't own the IO
  // task runner in unittests.
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceivePcmAudioMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write).WillOnce(Return(sizeof(HandshakePacket)));
  EXPECT_CALL(*socket, Read)
      // Ack message.
      .WillOnce(Invoke(
          [](net::IOBuffer* buf, int buf_len, net::CompletionOnceCallback) {
            int total_size = sizeof(HandshakePacket);
            EXPECT_GE(buf_len, total_size);
            FillBuffer(buf->data(), total_size, &kHandshakePacket.message_type,
                       sizeof(HandshakePacket) - sizeof(uint16_t));
            return total_size;
          }))
      // Audio message.
      .WillOnce(Invoke([](net::IOBuffer* buf, int buf_len,
                          net::CompletionOnceCallback) {
        int total_size = sizeof(PcmPacketHeader) + DataSizeInBytes(kStreamInfo);
        EXPECT_GE(buf_len, total_size);
        FillBuffer(buf->data(), total_size, &kPcmAudioPacketHeader.message_type,
                   sizeof(PcmPacketHeader) - sizeof(uint16_t));
        return total_size;  // No need to fill audio frames.
      }))
      .WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(delegate_, OnInitialStreamInfo).WillOnce(Return(true));
  EXPECT_CALL(delegate_, OnCaptureData).WillOnce(Return(true));

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
  // Stop receiver to disconnect socket, since receiver doesn't own the IO
  // task runner in unittests.
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveMetadataMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write).WillOnce(Return(sizeof(HandshakePacket)));
  EXPECT_CALL(*socket, Read)
      .WillOnce(Invoke(
          [](net::IOBuffer* buf, int buf_len, net::CompletionOnceCallback) {
            uint16_t size = sizeof(uint8_t) + 1;  // MessageType and 1 byte.
            int total_size = size + sizeof(size);
            EXPECT_GE(buf_len, total_size);
            base::WriteBigEndian(buf->data(), size);
            uint8_t message_type = static_cast<uint8_t>(MessageType::kMetadata);
            std::memcpy(buf->data() + sizeof(size), &message_type,
                        sizeof(message_type));
            return total_size;  // No need to fill metadata.
          }))
      .WillOnce(Return(net::ERR_IO_PENDING));
  // Neither OnCaptureError nor OnCaptureData will be called.
  EXPECT_CALL(delegate_, OnCaptureError).Times(0);
  EXPECT_CALL(delegate_, OnCaptureData).Times(0);
  EXPECT_CALL(delegate_, OnCaptureMetadata).Times(1);

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveError) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write).WillOnce(Return(sizeof(HandshakePacket)));
  EXPECT_CALL(*socket, Read).WillOnce(Return(net::ERR_CONNECTION_RESET));
  EXPECT_CALL(delegate_, OnCaptureError);

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveEosMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write).WillOnce(Return(sizeof(HandshakePacket)));
  EXPECT_CALL(*socket, Read).WillOnce(Return(0));
  EXPECT_CALL(delegate_, OnCaptureError);

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
