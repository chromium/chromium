// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/common/audio_decoder_software_wrapper.h"

#include "base/test/task_environment.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Field;
using ::testing::Return;

namespace chromecast {
namespace media {

namespace {

class MockAudioDecoder : public MediaPipelineBackend::AudioDecoder {
 public:
  MockAudioDecoder() { EXPECT_CALL(*this, SetDelegate(_)); }

  MOCK_METHOD1(SetDelegate, void(Delegate*));
  MOCK_METHOD1(PushBuffer, BufferStatus(CastDecoderBuffer*));
  MOCK_METHOD1(SetConfig, bool(const AudioConfig&));
  MOCK_METHOD1(SetVolume, bool(float));
  MOCK_METHOD0(GetRenderingDelay, RenderingDelay());
  MOCK_METHOD1(GetStatistics, void(Statistics*));
  MOCK_METHOD0(GetAudioTrackTimestamp, AudioTrackTimestamp());
  MOCK_METHOD0(GetStartThresholdInFrames, int());
};

}  // namespace

class AudioDecoderSoftwareWrapperTest : public ::testing::Test {
 public:
  AudioDecoderSoftwareWrapperTest()
      : audio_decoder_software_wrapper_(&audio_decoder_) {}

  base::test::TaskEnvironment task_environment_;
  MockAudioDecoder audio_decoder_;
  AudioDecoderSoftwareWrapper audio_decoder_software_wrapper_;
};

TEST_F(AudioDecoderSoftwareWrapperTest, IsUsingSoftwareDecoder) {
  AudioConfig audio_config;
  audio_config.channel_layout = ChannelLayout::STEREO;
  audio_config.sample_format = kSampleFormatS16;
  audio_config.bytes_per_channel = 2;
  audio_config.channel_number = 2;
  audio_config.samples_per_second = 48000;

  EXPECT_CALL(audio_decoder_, SetConfig(Field(&AudioConfig::codec, kCodecPCM)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(audio_decoder_, SetConfig(Field(&AudioConfig::codec, kCodecOpus)))
      .WillRepeatedly(Return(false));

  audio_config.codec = kCodecPCM;
  EXPECT_TRUE(audio_decoder_software_wrapper_.SetConfig(audio_config));
  EXPECT_FALSE(audio_decoder_software_wrapper_.IsUsingSoftwareDecoder());

  audio_config.codec = kCodecOpus;
  EXPECT_TRUE(audio_decoder_software_wrapper_.SetConfig(audio_config));
  EXPECT_TRUE(audio_decoder_software_wrapper_.IsUsingSoftwareDecoder());
}

}  // namespace media
}  // namespace chromecast
