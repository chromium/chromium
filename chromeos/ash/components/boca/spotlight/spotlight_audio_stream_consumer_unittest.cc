// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_audio_stream_consumer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "remoting/proto/audio.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca {
namespace {

using ::base::test::EqualsProto;

class SpotlightAudioStreamConsumerTest : public ::testing::Test {
 public:
  SpotlightAudioStreamConsumerTest() = default;
  ~SpotlightAudioStreamConsumerTest() override = default;

  void SetUp() override {
    audio_stream_consumer_ =
        std::make_unique<SpotlightAudioStreamConsumer>(base::BindRepeating(
            &SpotlightAudioStreamConsumerTest::HandleAudioPacket,
            base::Unretained(this)));
  }

  void HandleAudioPacket(std::unique_ptr<remoting::AudioPacket> packet) {
    received_packets_.push_back(std::move(packet));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SpotlightAudioStreamConsumer> audio_stream_consumer_;
  std::vector<std::unique_ptr<remoting::AudioPacket>> received_packets_;
};

TEST_F(SpotlightAudioStreamConsumerTest, ProcessAudioPacket) {
  remoting::AudioPacket expected_packet;
  expected_packet.set_timestamp(123);
  expected_packet.set_encoding(remoting::AudioPacket::ENCODING_RAW);
  expected_packet.set_sampling_rate(remoting::AudioPacket::SAMPLING_RATE_48000);
  expected_packet.set_bytes_per_sample(
      remoting::AudioPacket::BYTES_PER_SAMPLE_2);
  expected_packet.set_channels(remoting::AudioPacket::CHANNELS_STEREO);
  // Create a buffer of silent (zero) audio data.
  std::string audio_data(1024, 0);
  expected_packet.add_data(std::move(audio_data));

  bool done_called = false;
  audio_stream_consumer_->ProcessAudioPacket(
      std::make_unique<remoting::AudioPacket>(expected_packet),
      base::BindLambdaForTesting([&]() { done_called = true; }));

  EXPECT_TRUE(done_called);
  EXPECT_EQ(1u, received_packets_.size());
  EXPECT_THAT(expected_packet, EqualsProto(*received_packets_[0]));
}

}  // namespace
}  // namespace ash::boca
