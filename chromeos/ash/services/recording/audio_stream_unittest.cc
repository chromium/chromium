// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/services/recording/audio_stream.h"

#include "base/memory/aligned_memory.h"
#include "base/time/time.h"
#include "chromeos/ash/services/recording/audio_capture_test_base.h"
#include "chromeos/ash/services/recording/audio_capture_util.h"
#include "chromeos/ash/services/recording/recording_service_constants.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/vector_math.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace recording {

class AudioStreamTest : public AudioCaptureTestBase {
 public:
  AudioStreamTest() = default;
  AudioStreamTest(const AudioStreamTest&) = delete;
  AudioStreamTest& operator=(const AudioStreamTest&) = delete;
  ~AudioStreamTest() override = default;

  std::unique_ptr<media::AudioBus> ProduceAndAppendAudio(
      AudioStream& stream,
      base::TimeTicks timestamp) {
    auto stream_bus = ProduceAudio(timestamp);
    auto copy_bus = media::AudioBus::Create(audio_parameters_);
    stream_bus->CopyTo(copy_bus.get());
    stream.AppendAudioBus(std::move(stream_bus), timestamp);
    return copy_bus;
  }
};

TEST_F(AudioStreamTest, Basic) {
  AudioStream stream("");
  EXPECT_EQ(base::TimeTicks(), stream.begin_timestamp());
  EXPECT_EQ(base::TimeTicks(), stream.end_timestamp());
  EXPECT_EQ(0, stream.total_frames());

  ProduceAndAppendAudio(stream, GetTimestamp(base::Milliseconds(10)));
  EXPECT_EQ(GetTimestamp(base::Milliseconds(10)), stream.begin_timestamp());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(20)), stream.end_timestamp());
  EXPECT_EQ(audio_parameters_.frames_per_buffer(), stream.total_frames());

  // Using a wrong timestamp (e.g. earlier than the current `end_timestamp()` of
  // the stream), it will be ignored, and the bus will be appended at the
  // current end to guarantee monotonically increasing timestamps.
  ProduceAndAppendAudio(stream, GetTimestamp(base::Milliseconds(10)));
  EXPECT_EQ(GetTimestamp(base::Milliseconds(10)), stream.begin_timestamp());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(30)), stream.end_timestamp());
  EXPECT_EQ(2 * audio_parameters_.frames_per_buffer(), stream.total_frames());
}

TEST_F(AudioStreamTest, WrongTimestampWhenBusFullyConsumed) {
  AudioStream stream("");
  ProduceAndAppendAudio(stream, GetTimestamp(base::Milliseconds(10)));
  auto bus_for_consume = media::AudioBus::Create(audio_parameters_);
  stream.ConsumeAndAccumulateTo(
      /*destination=*/bus_for_consume.get(), /*destination_start_frame=*/0,
      /*frames_to_consume=*/bus_for_consume->frames());
  EXPECT_TRUE(stream.empty());
  EXPECT_EQ(stream.begin_timestamp(), stream.end_timestamp());
  EXPECT_EQ(stream.end_timestamp(), GetTimestamp(base::Milliseconds(20)));

  // Even though the stream is now empty, appending a bus with a wrong timestamp
  // should still work, and the timestamps of the stream should remain
  // monotonically increasing.
  ProduceAndAppendAudio(stream, GetTimestamp(base::Milliseconds(10)));
  EXPECT_EQ(GetTimestamp(base::Milliseconds(20)), stream.begin_timestamp());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(30)), stream.end_timestamp());
  EXPECT_EQ(audio_parameters_.frames_per_buffer(), stream.total_frames());
}

TEST_F(AudioStreamTest, GapsWillBeFilledWithZeros) {
  AudioStream stream("");
  EXPECT_EQ(base::TimeTicks(), stream.begin_timestamp());
  EXPECT_EQ(base::TimeTicks(), stream.end_timestamp());
  EXPECT_EQ(0, stream.total_frames());

  // Append two buses with a gap between both that is equal to the buffer
  // duration.
  //
  //       +----------+          +----------+
  //   10ms|          |      30ms|          |
  //       +----------+          +----------+
  //                        ^
  //                        |
  //                       Gap (will be filled with zeros).
  //
  ProduceAndAppendAudio(stream, GetTimestamp(base::Milliseconds(10)));
  ProduceAndAppendAudio(stream,
                        GetTimestamp(stream.end_timestamp().since_origin() +
                                     audio_parameters_.GetBufferDuration()));

  // It's as if a bus filled with zeros was appended in the middle.
  ASSERT_EQ(base::Milliseconds(10), audio_parameters_.GetBufferDuration());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(10)), stream.begin_timestamp());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(40)), stream.end_timestamp());
  EXPECT_EQ(3 * audio_parameters_.frames_per_buffer(), stream.total_frames());

  auto bus = media::AudioBus::Create(audio_parameters_);
  bus->Zero();
  stream.ConsumeAndAccumulateTo(bus.get(), /*destination_start_frame=*/0,
                                audio_parameters_.frames_per_buffer());
  // The first one is not zeros.
  EXPECT_FALSE(bus->AreFramesZero());
  // The begin timestamp is pushed after consume, the end timestamp remains the
  // same.
  EXPECT_EQ(GetTimestamp(base::Milliseconds(20)), stream.begin_timestamp());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(40)), stream.end_timestamp());

  // The second one is zero filled.
  bus->Zero();
  stream.ConsumeAndAccumulateTo(bus.get(), /*destination_start_frame=*/0,
                                audio_parameters_.frames_per_buffer());
  EXPECT_TRUE(bus->AreFramesZero());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(30)), stream.begin_timestamp());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(40)), stream.end_timestamp());

  // The third one is not zero filled.
  bus->Zero();
  stream.ConsumeAndAccumulateTo(bus.get(), /*destination_start_frame=*/0,
                                audio_parameters_.frames_per_buffer());
  EXPECT_FALSE(bus->AreFramesZero());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(40)), stream.begin_timestamp());
  EXPECT_EQ(GetTimestamp(base::Milliseconds(40)), stream.end_timestamp());
}

TEST_F(AudioStreamTest, ConsumeAccumulates) {
  AudioStream stream("");
  auto appended_bus =
      ProduceAndAppendAudio(stream, GetTimestamp(base::Milliseconds(10)));

  // The stream has a bus that has the same values as `appended_bus`. When we
  // consume the stream into `appended_bus`, it's like adding the frames of
  // `appended_bus` to themselves, effectively doubling the values.
  auto expected_bus = media::AudioBus::Create(audio_parameters_);
  appended_bus->CopyTo(expected_bus.get());
  expected_bus->Scale(2.0f);

  stream.ConsumeAndAccumulateTo(appended_bus.get(),
                                /*destination_start_frame=*/0,
                                appended_bus->frames());

  EXPECT_TRUE(AreBusesEqual(*expected_bus, *appended_bus));
}

TEST_F(AudioStreamTest, PartialConsumes) {
  AudioStream stream("");
  auto appended_bus =
      ProduceAndAppendAudio(stream, GetTimestamp(base::Milliseconds(10)));

  const int total_frames = appended_bus->frames();
  const int first_segment_frames = 0.3 * total_frames;
  const int second_segment_frames = total_frames - first_segment_frames;

  auto expected_first_segment =
      media::AudioBus::Create(appended_bus->channels(), first_segment_frames);
  auto expected_second_segment =
      media::AudioBus::Create(appended_bus->channels(), second_segment_frames);
  appended_bus->CopyPartialFramesTo(/*source_start_frame=*/0,
                                    /*frame_count=*/first_segment_frames,
                                    /*dest_start_frame=*/0,
                                    /*dest=*/expected_first_segment.get());
  appended_bus->CopyPartialFramesTo(/*source_start_frame=*/first_segment_frames,
                                    /*frame_count=*/second_segment_frames,
                                    /*dest_start_frame=*/0,
                                    /*dest=*/expected_second_segment.get());

  auto actual_first_segment =
      media::AudioBus::Create(appended_bus->channels(), first_segment_frames);
  actual_first_segment->Zero();
  auto actual_second_segment =
      media::AudioBus::Create(appended_bus->channels(), second_segment_frames);
  actual_second_segment->Zero();

  stream.ConsumeAndAccumulateTo(/*destination=*/actual_first_segment.get(),
                                /*destination_start_frame=*/0,
                                /*frames_to_consume=*/first_segment_frames);
  // After consuming a number of frames from the stream that is equal to the
  // number of frames in the first segment, the remaining total number of frames
  // in the stream is equal to the number of frames in the second segment.
  EXPECT_EQ(second_segment_frames, stream.total_frames());

  stream.ConsumeAndAccumulateTo(/*destination=*/actual_second_segment.get(),
                                /*destination_start_frame=*/0,
                                /*frames_to_consume=*/second_segment_frames);
  EXPECT_TRUE(stream.empty());

  EXPECT_TRUE(AreBusesEqual(*expected_first_segment, *actual_first_segment));
  EXPECT_TRUE(AreBusesEqual(*expected_second_segment, *actual_second_segment));
}

TEST_F(AudioStreamTest, ConsumeToMisalignedDestination) {
  AudioStream stream("");
  auto appended_bus =
      ProduceAndAppendAudio(stream, GetTimestamp(base::Milliseconds(10)));

  auto destination =
      audio_capture_util::CreateStereoZeroInitializedAudioBusForFrames(
          2 * appended_bus->frames());

  // Find the first misaligned frame, so that we can use it as the start frame
  // to consume to in the `destination`.
  int mis_aligned_start_frame = 0;
  for (; mis_aligned_start_frame < destination->frames();
       ++mis_aligned_start_frame) {
    if (!base::IsAligned(&destination->channel(0)[mis_aligned_start_frame],
                         media::vector_math::kRequiredAlignment)) {
      break;
    }
  }

  stream.ConsumeAndAccumulateTo(
      /*destination=*/destination.get(),
      /*destination_start_frame=*/mis_aligned_start_frame,
      /*frames_to_consume=*/appended_bus->frames());
  EXPECT_TRUE(stream.empty());

  // This bus contains the frames we consumed from the stream into destination
  // starting at the misaligned address. It should be exactly equal to the
  // `appended_bus`.
  auto consumed_frames = media::AudioBus::Create(audio_parameters_);
  destination->CopyPartialFramesTo(
      /*source_start_frame=*/mis_aligned_start_frame,
      /*frame_count=*/appended_bus->frames(),
      /*dest_start_frame=*/0,
      /*dest=*/consumed_frames.get());

  EXPECT_TRUE(AreBusesEqual(*consumed_frames, *appended_bus));
}

}  // namespace recording
