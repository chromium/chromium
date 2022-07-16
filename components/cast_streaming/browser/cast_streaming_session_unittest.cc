// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_streaming_session.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_streaming/browser/test/cast_streaming_test_receiver.h"
#include "components/cast_streaming/browser/test/cast_streaming_test_sender.h"
#include "media/base/media_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {

namespace {

media::AudioDecoderConfig GetDefaultAudioConfig() {
  return media::AudioDecoderConfig(
      media::AudioCodec::kOpus, media::SampleFormat::kSampleFormatF32,
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
      48000 /* samples_per_second */, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);
}

media::VideoDecoderConfig GetDefaultVideoConfig() {
  const gfx::Size kVideoSize = {1920, 1080};
  const gfx::Rect kVideoRect(kVideoSize);

  return media::VideoDecoderConfig(
      media::VideoCodec::kVP8, media::VideoCodecProfile::VP8PROFILE_MIN,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::VideoTransformation(), kVideoSize, kVideoRect, kVideoSize,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);
}

// Compare |decoder_buffer| to |data_buffer| metadata and data. Returns true if
// these are equivalent.
void VerifyAreEquals(const scoped_refptr<media::DecoderBuffer>& decoder_buffer,
                     const scoped_refptr<media::DataBuffer>& data_buffer) {
  if (decoder_buffer->end_of_stream() || data_buffer->end_of_stream()) {
    EXPECT_EQ(decoder_buffer->end_of_stream(), data_buffer->end_of_stream());
    return;
  }

  ASSERT_EQ(decoder_buffer->timestamp(), data_buffer->timestamp());

  // Signed to unsigned conversion.
  size_t data_buffer_size = data_buffer->data_size();
  ASSERT_EQ(decoder_buffer->data_size(), data_buffer_size);

  ASSERT_EQ(
      memcmp(decoder_buffer->data(), data_buffer->data(), data_buffer_size), 0);
}

}  // namespace

class CastStreamingSessionTest : public testing::Test {
 public:
  CastStreamingSessionTest() = default;
  ~CastStreamingSessionTest() override = default;

  CastStreamingSessionTest(const CastStreamingSessionTest&) = delete;
  CastStreamingSessionTest& operator=(const CastStreamingSessionTest&) = delete;

 protected:
  void StartSession() {
    std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port;
    std::unique_ptr<cast_api_bindings::MessagePort> receiver_message_port;
    cast_api_bindings::CreatePlatformMessagePortPair(&sender_message_port,
                                                     &receiver_message_port);

    receiver_.Start(std::move(receiver_message_port));
    EXPECT_TRUE(sender_.Start(
        std::move(sender_message_port), net::IPAddress::IPv6Localhost(),
        GetDefaultAudioConfig(), GetDefaultVideoConfig()));
    sender_.RunUntilStarted();
    receiver_.RunUntilStarted();
  }

  void StopReceiverSession() {
    receiver_.Stop();
    receiver_.RunUntilStopped();
    sender_.RunUntilStopped();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  CastStreamingTestSender sender_;
  CastStreamingTestReceiver receiver_;
};

// Basic sanity check for the test fixture.
TEST_F(CastStreamingSessionTest, DoNothing) {}

// Tests session lifespan is correct.
TEST_F(CastStreamingSessionTest, StartAndStopSession) {
  StartSession();

  // Sender-initiated closure.
  sender_.Stop();
  sender_.RunUntilStopped();
  receiver_.RunUntilStopped();

  // Start a new session.
  StartSession();

  // Renderer-initiated closure.
  StopReceiverSession();
}

// Tests buffers sent from the sender are properly received by the receiver.
TEST_F(CastStreamingSessionTest, SendAndReceiveBuffers) {
  StartSession();

  const uint8_t kAudioData[] = {42};
  scoped_refptr<media::DataBuffer> audio_buffer =
      media::DataBuffer::CopyFrom(kAudioData, sizeof(kAudioData));
  audio_buffer->set_timestamp(base::Seconds(0));

  const uint8_t kVideoData[] = {42, 84};
  scoped_refptr<media::DataBuffer> video_buffer =
      media::DataBuffer::CopyFrom(kVideoData, sizeof(kVideoData));
  video_buffer->set_timestamp(base::Seconds(0));

  sender_.SendAudioBuffer(audio_buffer);
  sender_.SendVideoBuffer(video_buffer, true);

  ASSERT_TRUE(receiver_.RunUntilAudioFramesCountIsAtLeast(1u));
  VerifyAreEquals(receiver_.audio_buffers()[0], audio_buffer);

  ASSERT_TRUE(receiver_.RunUntilVideoFramesCountIsAtLeast(1u));
  VerifyAreEquals(receiver_.video_buffers()[0], video_buffer);
  EXPECT_TRUE(receiver_.video_buffers()[0]->is_key_frame());

  StopReceiverSession();
}

}  // namespace cast_streaming
