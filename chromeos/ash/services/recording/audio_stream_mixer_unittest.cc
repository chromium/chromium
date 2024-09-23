// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chromeos/ash/services/recording/audio_capture_test_base.h"
#include "chromeos/ash/services/recording/audio_capture_util.h"
#include "chromeos/ash/services/recording/audio_stream.h"
#include "chromeos/ash/services/recording/audio_stream_mixer.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace recording {

namespace {

// Adds the frames in `bus1` and `bus2` together and returns a new audio bus
// that contains the sum. Both `bus1` and `bus2` must have the same number of
// channels and frames.
std::unique_ptr<media::AudioBus> AddBuses(const media::AudioBus& bus1,
                                          const media::AudioBus& bus2) {
  DCHECK_EQ(bus1.channels(), bus2.channels());
  DCHECK_EQ(bus1.frames(), bus2.frames());

  auto output_bus = media::AudioBus::Create(bus1.channels(), bus1.frames());
  for (int i = 0; i < output_bus->channels(); ++i) {
    const auto* const bus1_channel = bus1.channel(i);
    const auto* const bus2_channel = bus2.channel(i);
    auto* const output_bus_channel = output_bus->channel(i);
    for (int j = 0; j < bus1.frames(); ++j) {
      output_bus_channel[j] = bus1_channel[j] + bus2_channel[j];
    }
  }

  return output_bus;
}

}  // namespace

// An instance of this class can be used as a client of an `AudioStreamMixer`
// which will be provided with the output audio buses that contain the mixed
// audio streams.
class MixedOutputReceiver {
 public:
  MixedOutputReceiver() = default;
  MixedOutputReceiver(const MixedOutputReceiver&) = delete;
  MixedOutputReceiver& operator=(const MixedOutputReceiver&) = delete;
  ~MixedOutputReceiver() = default;

  const media::AudioBus* last_mixer_bus() const {
    return last_mixer_bus_.get();
  }
  base::TimeTicks last_bus_timestamp() const { return last_bus_timestamp_; }

  // Creates a callback that can be provided to the constructor of
  // `AudioStreamMixer`.
  OnAudioMixerOutputCallback GetCallback() {
    return base::BindRepeating(&MixedOutputReceiver::OnMixerOutput,
                               base::Unretained(this));
  }

 private:
  void OnMixerOutput(std::unique_ptr<media::AudioBus> audio_bus,
                     base::TimeTicks timestamp) {
    last_mixer_bus_ = std::move(audio_bus);
    last_bus_timestamp_ = timestamp;
  }

  // The audio bus that was most recently received from the `AudioStreamMixer`
  // that this object is a client of.
  std::unique_ptr<media::AudioBus> last_mixer_bus_;

  // The timestamp of the very first frame of the above audio bus.
  base::TimeTicks last_bus_timestamp_;
};

class AudioStreamMixerTest : public AudioCaptureTestBase {
 public:
  AudioStreamMixerTest() = default;
  AudioStreamMixerTest(const AudioStreamMixerTest&) = delete;
  AudioStreamMixerTest& operator=(const AudioStreamMixerTest&) = delete;
  ~AudioStreamMixerTest() override = default;

  static AudioStreamMixer::PassKey PassKey() {
    return AudioStreamMixer::PassKeyForTesting();
  }

  // Adds a new stream to the given `stream_mixer` and returns a reference to
  // it.
  AudioStream* AddStreamToMixer(AudioStreamMixer& stream_mixer) {
    stream_mixer.streams_.push_back(std::make_unique<AudioStream>(""));
    return stream_mixer.streams_.back().get();
  }

  // Simulates receiving a captured-at-`timestamp` audio bus from the capturer
  // associated with the given `stream` that belongs to the given
  // `stream_mixer`.
  // Returns an exact copy of that audio bus so that tests can use it for
  // comparisons.
  std::unique_ptr<media::AudioBus> ProduceAudioForStream(
      AudioStreamMixer& stream_mixer,
      AudioStream* stream,
      base::TimeTicks timestamp) {
    auto stream_bus = ProduceAudio(timestamp);
    auto copy_bus = media::AudioBus::Create(audio_parameters_);
    stream_bus->CopyTo(copy_bus.get());
    stream_mixer.OnAudioCaptured(stream, std::move(stream_bus), timestamp);
    return copy_bus;
  }

  // Flushes the given `stream_mixer` by mixing all the available frames and
  // providing the output to the client.
  void FlushMixer(AudioStreamMixer& stream_mixer) {
    stream_mixer.MaybeMixAndOutput(/*flush=*/true);
  }
};

// When there's only a single stream, the mixer behaves as a pass-through, and
// all the received audio buses from the capturer are consumed and delivered to
// the client immediately.
TEST_F(AudioStreamMixerTest, SingleStream) {
  MixedOutputReceiver mixer_client;
  AudioStreamMixer mixer(PassKey(), mixer_client.GetCallback());
  AudioStream* stream1 = AddStreamToMixer(mixer);

  auto timestamp = GetTimestamp(base::Milliseconds(10));
  auto expected_bus = ProduceAudioForStream(mixer, stream1, timestamp);

  ASSERT_TRUE(mixer_client.last_mixer_bus());
  EXPECT_TRUE(AreBusesEqual(*expected_bus, *mixer_client.last_mixer_bus()));
  EXPECT_EQ(timestamp, mixer_client.last_bus_timestamp());
  EXPECT_TRUE(stream1->empty());

  timestamp = GetTimestamp(base::Milliseconds(20));
  expected_bus = ProduceAudioForStream(mixer, stream1, timestamp);

  EXPECT_TRUE(AreBusesEqual(*expected_bus, *mixer_client.last_mixer_bus()));
  EXPECT_EQ(timestamp, mixer_client.last_bus_timestamp());
  EXPECT_TRUE(stream1->empty());
}

TEST_F(AudioStreamMixerTest, TwoStreamsPerfectlyAligned) {
  MixedOutputReceiver mixer_client;
  AudioStreamMixer mixer(PassKey(), mixer_client.GetCallback());
  AudioStream* stream1 = AddStreamToMixer(mixer);
  AudioStream* stream2 = AddStreamToMixer(mixer);

  const auto timestamp = GetTimestamp(base::Milliseconds(10));
  auto stream1_bus = ProduceAudioForStream(mixer, stream1, timestamp);
  auto stream2_bus = ProduceAudioForStream(mixer, stream2, timestamp);

  // Since both streams have buffers that are perfectly aligned, the expected
  // mixer output bus should be an exact sum of the two input buses.
  auto expected_bus = AddBuses(*stream1_bus, *stream2_bus);
  ASSERT_TRUE(mixer_client.last_mixer_bus());
  EXPECT_TRUE(AreBusesEqual(*expected_bus, *mixer_client.last_mixer_bus()));
  EXPECT_EQ(timestamp, mixer_client.last_bus_timestamp());
}

TEST_F(AudioStreamMixerTest, StreamWithLaterTimestampsArrivesFirst) {
  MixedOutputReceiver mixer_client;
  AudioStreamMixer mixer(PassKey(), mixer_client.GetCallback());
  AudioStream* stream1 = AddStreamToMixer(mixer);
  AudioStream* stream2 = AddStreamToMixer(mixer);

  // Stream 1                                  +-------+
  //                                      40ms |       |
  //                                           +-------+
  //
  // Stream 2
  //               EMPTY
  //
  //
  auto stream1_bus = ProduceAudioForStream(
      mixer, stream1, GetTimestamp(base::Milliseconds(40)));
  // Nothing gets mixed yet.
  EXPECT_FALSE(mixer_client.last_mixer_bus());
  EXPECT_TRUE(mixer_client.last_bus_timestamp().is_null());

  auto stream2_bus1 = ProduceAudioForStream(
      mixer, stream2, GetTimestamp(base::Milliseconds(10)));

  // Stream 1                                  +-------+
  // Arrived first:                       40ms |       |
  //                                           +-------+
  //
  // Stream 2              +-------+
  // Arrived later:   10ms |       |
  //                       +-------+
  //
  // Both streams has frames now, but there's no overlap, so stream2's bus will
  // be the output of the mixer.
  ASSERT_TRUE(mixer_client.last_mixer_bus());
  EXPECT_TRUE(AreBusesEqual(*stream2_bus1, *mixer_client.last_mixer_bus()));
  EXPECT_EQ(GetTimestamp(base::Milliseconds(10)),
            mixer_client.last_bus_timestamp());

  // New buffer arrives on stream2 at 45ms.
  //
  // Stream 1                                  +-------+
  //                                      40ms |       |
  //                                           +-------+
  //
  // Stream 2              +-------+               +-------+
  //                  10ms |       |          45ms |       |
  //                       +-------+               +-------+
  //                           ^   ^           ^       ^   ^
  //                           |  20ms         +---+---+   55ms
  //                    consumed              40ms |  50ms
  //                                               |
  //                                     to be mixed and consumed
  //
  // There is an overlap up until half stream1's bus at 50ms.
  auto stream2_bus2 = ProduceAudioForStream(
      mixer, stream2, GetTimestamp(base::Milliseconds(45)));
  EXPECT_EQ(GetTimestamp(base::Milliseconds(40)),
            mixer_client.last_bus_timestamp());

  auto expected_bus =
      audio_capture_util::CreateStereoZeroInitializedAudioBusForDuration(
          base::Milliseconds(50 - 40));
  EXPECT_EQ(expected_bus->frames(), mixer_client.last_mixer_bus()->frames());
  stream1_bus->CopyPartialFramesTo(
      /*source_start_frame=*/0,
      /*frame_count=*/stream1_bus->frames(),
      /*dest_start_frame=*/0, expected_bus.get());
  audio_capture_util::AccumulateBusTo(
      /*source=*/*stream2_bus2,
      /*destination=*/expected_bus.get(),
      /*source_start_frame=*/0,
      /*destination_start_frame=*/
      audio_capture_util::NumberOfAudioFramesInDuration(
          base::Milliseconds(45 - 40)),
      /*length=*/
      audio_capture_util::NumberOfAudioFramesInDuration(
          base::Milliseconds(50 - 45)));
  EXPECT_TRUE(AreBusesEqual(*expected_bus, *mixer_client.last_mixer_bus()));

  // Stream 2 was partially consumed, so check that the left over frames are
  // there.
  EXPECT_EQ(stream2->total_frames(),
            audio_capture_util::NumberOfAudioFramesInDuration(
                base::Milliseconds(55 - 50)));
  EXPECT_EQ(stream2->begin_timestamp(), GetTimestamp(base::Milliseconds(50)));
  EXPECT_EQ(stream2->end_timestamp(), GetTimestamp(base::Milliseconds(55)));
}

TEST_F(AudioStreamMixerTest, OneStreamReachedMaxDuration) {
  MixedOutputReceiver mixer_client;
  AudioStreamMixer mixer(PassKey(), mixer_client.GetCallback());
  AudioStream* stream1 = AddStreamToMixer(mixer);
  AddStreamToMixer(mixer);

  int next_milliseconds = 10;
  while (true) {
    ProduceAudioForStream(mixer, stream1,
                          GetTimestamp(base::Milliseconds(next_milliseconds)));
    next_milliseconds += 10;

    if (!stream1->empty() &&
        stream1->total_frames() <
            audio_capture_util::NumberOfAudioFramesInDuration(
                audio_capture_util::kMaxAudioStreamFifoDuration)) {
      // Nothing gets mixed yet.
      ASSERT_FALSE(mixer_client.last_mixer_bus());
      EXPECT_TRUE(mixer_client.last_bus_timestamp().is_null());
    } else {
      break;
    }
  }

  EXPECT_TRUE(mixer_client.last_mixer_bus());
  EXPECT_EQ(mixer_client.last_bus_timestamp(),
            GetTimestamp(base::Milliseconds(10)));
  EXPECT_EQ(mixer_client.last_mixer_bus()->frames(),
            audio_capture_util::NumberOfAudioFramesInDuration(
                audio_capture_util::kMaxAudioStreamFifoDuration));
}

TEST_F(AudioStreamMixerTest, FlushingTheMixer) {
  // Build a mixer that has 3 streams, one of them is always empty.
  MixedOutputReceiver mixer_client;
  AudioStreamMixer mixer(PassKey(), mixer_client.GetCallback());
  AudioStream* stream1 = AddStreamToMixer(mixer);
  AudioStream* stream2 = AddStreamToMixer(mixer);
  AddStreamToMixer(mixer);

  auto stream1_bus = ProduceAudioForStream(
      mixer, stream1, GetTimestamp(base::Milliseconds(10)));
  // Nothing gets mixed yet since stream 2 and 3 are still empty.
  EXPECT_FALSE(mixer_client.last_mixer_bus());
  EXPECT_TRUE(mixer_client.last_bus_timestamp().is_null());

  auto stream2_bus = ProduceAudioForStream(
      mixer, stream2, GetTimestamp(base::Milliseconds(40)));
  // Nothing gets mixed yet since stream 3 is still empty.
  EXPECT_FALSE(mixer_client.last_mixer_bus());
  EXPECT_TRUE(mixer_client.last_bus_timestamp().is_null());

  //
  // Stream 1              +-------+
  //                  10ms |       |
  //                       +-------+
  //
  // Stream 2                                  +-------+
  //                                      40ms |       |
  //                                           +-------+
  //
  // Stream 3
  //                       EMPTY
  //

  // When we request to flush the stream, all available buffers from start to
  // end will be mixed regardless of the empty stream.
  auto expected_bus =
      audio_capture_util::CreateStereoZeroInitializedAudioBusForDuration(
          base::Milliseconds(50 - 10));
  stream1_bus->CopyPartialFramesTo(
      /*source_start_frame=*/0,
      /*frame_count=*/stream1_bus->frames(),
      /*dest_start_frame=*/0, expected_bus.get());
  audio_capture_util::AccumulateBusTo(
      /*source=*/*stream2_bus,
      /*destination=*/expected_bus.get(),
      /*source_start_frame=*/0,
      /*destination_start_frame=*/
      audio_capture_util::NumberOfAudioFramesInDuration(
          base::Milliseconds(40 - 10)),
      /*length=*/stream2_bus->frames());

  FlushMixer(mixer);
  ASSERT_TRUE(mixer_client.last_mixer_bus());
  EXPECT_EQ(expected_bus->frames(), mixer_client.last_mixer_bus()->frames());
  EXPECT_TRUE(AreBusesEqual(*expected_bus, *mixer_client.last_mixer_bus()));
  EXPECT_EQ(GetTimestamp(base::Milliseconds(10)),
            mixer_client.last_bus_timestamp());
}

}  // namespace recording
