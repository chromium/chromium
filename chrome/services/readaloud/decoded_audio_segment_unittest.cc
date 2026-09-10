// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoded_audio_segment.h"

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chrome/services/readaloud/word_timing.h"
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

  std::vector<WordTiming> timings = {{.start_time = base::Milliseconds(0),
                                      .end_time = base::Milliseconds(200),
                                      .start_character_offset = 0u,
                                      .end_character_offset = 5u},
                                     {.start_time = base::Milliseconds(200),
                                      .end_time = base::Milliseconds(500),
                                      .start_character_offset = 6u,
                                      .end_character_offset = 11u}};

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
              testing::Field(&WordTiming::start_character_offset, 0u),
              testing::Field(&WordTiming::end_character_offset, 5u),
              testing::Field(&WordTiming::start_time, base::Milliseconds(0)),
              testing::Field(&WordTiming::end_time, base::Milliseconds(200))),
          testing::AllOf(
              testing::Field(&WordTiming::start_character_offset, 6u),
              testing::Field(&WordTiming::end_character_offset, 11u),
              testing::Field(&WordTiming::start_time, base::Milliseconds(200)),
              testing::Field(&WordTiming::end_time, base::Milliseconds(500)))));
}

}  // namespace readaloud
