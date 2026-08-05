// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoded_audio_segment.h"

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "media/base/audio_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

TEST(DecodedAudioSegmentTest, DefaultConstructor) {
  auto segment = base::MakeRefCounted<DecodedAudioSegment>();
  EXPECT_EQ(nullptr, segment->audio_buffer());
  EXPECT_EQ(0, segment->sample_rate());
  EXPECT_EQ(base::TimeDelta(), segment->duration());
  EXPECT_TRUE(segment->word_timings().empty());
}

TEST(DecodedAudioSegmentTest, DurationConstructor) {
  auto segment =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(500));
  EXPECT_EQ(nullptr, segment->audio_buffer());
  EXPECT_EQ(0, segment->sample_rate());
  EXPECT_EQ(base::Milliseconds(500), segment->duration());
  EXPECT_TRUE(segment->word_timings().empty());
}

TEST(DecodedAudioSegmentTest, FullConstructor) {
  constexpr int kSampleRate = 44100;
  constexpr int kChannels = 2;
  constexpr int kFrames = 22050;
  auto buffer = media::AudioBuffer::CreateEmptyBuffer(
      media::CHANNEL_LAYOUT_STEREO, kChannels, kSampleRate, kFrames,
      base::TimeDelta());

  std::vector<DecodedAudioSegment::WordTiming> timings = {
      {"Hello", base::Milliseconds(0), base::Milliseconds(200)},
      {"World", base::Milliseconds(200), base::Milliseconds(500)}};

  auto segment =
      base::MakeRefCounted<DecodedAudioSegment>(std::move(buffer), timings);

  ASSERT_NE(nullptr, segment->audio_buffer());
  EXPECT_EQ(kChannels, segment->audio_buffer()->channel_count());
  EXPECT_EQ(kFrames, segment->audio_buffer()->frame_count());
  EXPECT_EQ(kSampleRate, segment->sample_rate());
  EXPECT_EQ(base::Milliseconds(500), segment->duration());
  ASSERT_EQ(2u, segment->word_timings().size());
  EXPECT_THAT(
      segment->word_timings(),
      testing::ElementsAre(
          testing::AllOf(
              testing::Field(&DecodedAudioSegment::WordTiming::text, "Hello"),
              testing::Field(&DecodedAudioSegment::WordTiming::start_time,
                             base::Milliseconds(0)),
              testing::Field(&DecodedAudioSegment::WordTiming::end_time,
                             base::Milliseconds(200))),
          testing::AllOf(
              testing::Field(&DecodedAudioSegment::WordTiming::text, "World"),
              testing::Field(&DecodedAudioSegment::WordTiming::start_time,
                             base::Milliseconds(200)),
              testing::Field(&DecodedAudioSegment::WordTiming::end_time,
                             base::Milliseconds(500)))));
}

}  // namespace readaloud
