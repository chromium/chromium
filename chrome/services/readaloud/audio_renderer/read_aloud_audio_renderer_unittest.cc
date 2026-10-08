// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"

#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/services/readaloud/audio_renderer/word_boundary_queue.h"
#include "chrome/services/readaloud/audio_segment_queue.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "chrome/services/readaloud/word_timing.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

namespace {

// A word boundary as reported through the renderer's WordBoundaryCallback.
struct Boundary {
  uint32_t start_character_offset = 0;
  uint32_t end_character_offset = 0;
  base::TimeDelta audio_timestamp;

  friend bool operator==(const Boundary&, const Boundary&) = default;

  friend void PrintTo(const Boundary& boundary, std::ostream* os) {
    *os << "{start: " << boundary.start_character_offset
        << ", end: " << boundary.end_character_offset
        << ", audio_timestamp: " << boundary.audio_timestamp << "}";
  }
};

WordTiming MakeWord(base::TimeDelta start_time,
                    uint32_t start_character_offset,
                    uint32_t end_character_offset) {
  WordTiming word;
  word.start_time = start_time;
  word.end_time = start_time + base::Milliseconds(1);
  word.start_character_offset = start_character_offset;
  word.end_character_offset = end_character_offset;
  return word;
}

// Returns the boundary the renderer reports for `word` when `audio_timestamp`
// is the media time audible at dispatch.
Boundary BoundaryFor(const WordTiming& word, base::TimeDelta audio_timestamp) {
  return {.start_character_offset = word.start_character_offset,
          .end_character_offset = word.end_character_offset,
          .audio_timestamp = audio_timestamp};
}

}  // namespace

class ReadAloudAudioRendererTest : public testing::Test {
 protected:
  void SetUp() override {
    queue_ = std::make_unique<AudioSegmentQueue>();
    renderer_ = std::make_unique<ReadAloudAudioRenderer>();
  }

  // 48 kHz stereo with 480-frame (10 ms) buffers.
  static media::AudioParameters MakeParams() {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::ChannelLayoutConfig::Stereo(),
                                  /*sample_rate=*/48000,
                                  /*frames_per_buffer=*/480);
  }

  scoped_refptr<DecodedAudioSegment> GenerateSegment(
      const media::AudioParameters& params,
      int frames,
      float base_value = 0.0f,
      float step = 0.001f) {
    auto bus = media::AudioBus::Create(params.channels(), frames);
    for (int i = 0; i < frames; ++i) {
      float val = base_value + i * step;
      if (bus->channels() > 0) {
        bus->channel(0)[i] = val;
      }
      if (bus->channels() > 1) {
        bus->channel(1)[i] = -val;
      }
    }
    auto buffer = media::AudioBuffer::CopyFrom(params.sample_rate(),
                                               base::TimeDelta(), bus.get());
    return base::MakeRefCounted<DecodedAudioSegment>(std::move(buffer));
  }

  // Returns the number of frames that last `duration` at `params`' rate.
  static int FramesFor(const media::AudioParameters& params,
                       base::TimeDelta duration) {
    return base::checked_cast<int>(media::AudioTimestampHelper::TimeToFrames(
        duration, params.sample_rate()));
  }

  // Generates a segment of `frames` frames, as GenerateSegment() does, that
  // carries `timings`.
  scoped_refptr<DecodedAudioSegment> GenerateSegmentWithTimings(
      const media::AudioParameters& params,
      int frames,
      std::vector<WordTiming> timings) {
    return base::MakeRefCounted<DecodedAudioSegment>(
        GenerateSegment(params, frames)->audio_buffer(), std::move(timings));
  }

  // Records every boundary the renderer dispatches into `boundaries_`.
  void RecordBoundaries() {
    renderer_->SetWordBoundaryCallback(base::BindRepeating(
        [](std::vector<Boundary>* out, uint32_t start, uint32_t end,
           base::TimeDelta audio_timestamp) {
          out->push_back({start, end, audio_timestamp});
        },
        &boundaries_));
  }

  // Renders one buffer of `params.frames_per_buffer()` frames with the given
  // output `delay`.
  void RenderOnce(const media::AudioParameters& params,
                  base::TimeDelta delay = base::TimeDelta()) {
    std::unique_ptr<media::AudioBus> dest = media::AudioBus::Create(params);
    renderer_->Render(delay, /*delay_timestamp=*/base::TimeTicks::Now(),
                      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());
  }

  // Verifies that a range of the destination AudioBus matches the expected
  // pattern generated by `GenerateSegment` (accounting for frame offsets).
  void VerifySegment(const media::AudioBus& bus,
                     int start_frame,
                     int end_frame,
                     int start_frame_offset,
                     float base_value = 0.0f,
                     float step = 0.001f) {
    for (int i = start_frame; i < end_frame; ++i) {
      float expected_val =
          base_value + (i - start_frame + start_frame_offset) * step;
      if (bus.channels() > 0) {
        ASSERT_NEAR(bus.channel(0)[i], expected_val, 1e-5f)
            << "Left channel mismatch at frame " << i;
      }
      if (bus.channels() > 1) {
        ASSERT_NEAR(bus.channel(1)[i], -expected_val, 1e-5f)
            << "Right channel mismatch at frame " << i;
      }
    }
  }

  // Word boundaries are dispatched by delayed tasks scheduled against the
  // audio clock, so the tests control mock time to observe them.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<AudioSegmentQueue> queue_;
  // Outlives `renderer_`, whose callback points at it.
  std::vector<Boundary> boundaries_;
  std::unique_ptr<ReadAloudAudioRenderer> renderer_;
};

TEST_F(ReadAloudAudioRendererTest, LifecycleInitializeValid) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  EXPECT_TRUE(params.IsValid());
  EXPECT_TRUE(renderer_->Initialize(params, queue_.get()));
}

TEST_F(ReadAloudAudioRendererTest, LifecycleInitializeInvalid) {
  // Invalid sample rate
  media::AudioParameters invalid_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 0, 480);
  EXPECT_FALSE(invalid_params.IsValid());
  EXPECT_FALSE(renderer_->Initialize(invalid_params, queue_.get()));

  // Null queue
  media::AudioParameters valid_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  EXPECT_TRUE(valid_params.IsValid());
  EXPECT_FALSE(renderer_->Initialize(valid_params, nullptr));
}

TEST_F(ReadAloudAudioRendererTest, RenderReturnsSilenceWhenQueueIsEmpty) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));

  auto dest = media::AudioBus::Create(params);
  // Pre-fill destination with non-zero values to ensure we can verify it gets
  // zeroed.
  for (int c = 0; c < dest->channels(); ++c) {
    for (int i = 0; i < dest->frames(); ++i) {
      dest->channel(c)[i] = 1.0f;
    }
  }

  int frames_rendered = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());

  EXPECT_EQ(frames_rendered, 0);

  // Verify the destination buffer was zeroed out.
  EXPECT_TRUE(dest->AreFramesZero());
}

TEST_F(ReadAloudAudioRendererTest, RenderWithoutInitializeZeroesBuffer) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  auto dest = media::AudioBus::Create(params);
  // Pre-fill destination with non-zero values.
  for (int c = 0; c < dest->channels(); ++c) {
    for (int i = 0; i < dest->frames(); ++i) {
      dest->channel(c)[i] = 1.0f;
    }
  }

  int frames_rendered = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());

  EXPECT_EQ(frames_rendered, 0);

  // Verify the destination buffer was zeroed out because it is not initialized.
  EXPECT_TRUE(dest->AreFramesZero());
}

TEST_F(ReadAloudAudioRendererTest, RenderMatchesSegmentSize) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));

  // Create a segment of 480 frames.
  auto segment = GenerateSegment(params, /*frames=*/480);
  ASSERT_TRUE(queue_->Push(segment));

  auto dest = media::AudioBus::Create(params);
  int frames_rendered = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());

  EXPECT_EQ(frames_rendered, 480);
  VerifySegment(*dest, /*start_frame=*/0, /*end_frame=*/dest->frames(),
                /*start_frame_offset=*/0);
}

TEST_F(ReadAloudAudioRendererTest, RenderPartialCopies) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));

  // Create a segment of 960 frames.
  auto segment = GenerateSegment(params, /*frames=*/960);
  ASSERT_TRUE(queue_->Push(segment));

  // Render first half
  auto dest1 = media::AudioBus::Create(params);
  int frames_rendered_1 = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest1.get());
  EXPECT_EQ(frames_rendered_1, 480);
  VerifySegment(*dest1, /*start_frame=*/0, /*end_frame=*/dest1->frames(),
                /*start_frame_offset=*/0);

  // Render second half
  auto dest2 = media::AudioBus::Create(params);
  int frames_rendered_2 = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest2.get());
  EXPECT_EQ(frames_rendered_2, 480);
  VerifySegment(*dest2, /*start_frame=*/0, /*end_frame=*/dest2->frames(),
                /*start_frame_offset=*/480);
}

TEST_F(ReadAloudAudioRendererTest, RenderMultipleSegments) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));

  // Create segment 1 of 200 frames.
  auto segment1 = GenerateSegment(params, /*frames=*/200, /*base_value=*/0.1f);
  ASSERT_TRUE(queue_->Push(segment1));

  // Create segment 2 of 200 frames.
  auto segment2 = GenerateSegment(params, /*frames=*/200, /*base_value=*/0.5f);
  ASSERT_TRUE(queue_->Push(segment2));

  // Destination of 480 frames, pre-filled with -1.0f.
  auto dest = media::AudioBus::Create(params);
  for (int c = 0; c < dest->channels(); ++c) {
    for (int i = 0; i < dest->frames(); ++i) {
      dest->channel(c)[i] = -1.0f;
    }
  }

  int frames_rendered = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());

  // We only had 400 frames available across both segments.
  EXPECT_EQ(frames_rendered, 400);

  // Verify segment 1, segment 2, and silence ranges.
  VerifySegment(*dest, /*start_frame=*/0, /*end_frame=*/200,
                /*start_frame_offset=*/0, /*base_value=*/0.1f);
  VerifySegment(*dest, /*start_frame=*/200, /*end_frame=*/400,
                /*start_frame_offset=*/0, /*base_value=*/0.5f);
  VerifySegment(*dest, /*start_frame=*/400, /*end_frame=*/480,
                /*start_frame_offset=*/0, /*base_value=*/0.0f, /*step=*/0.0f);
}

TEST_F(ReadAloudAudioRendererTest, RenderTimeStretchingFaster) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));

  // Set playback rate to 2.0x (double speed).
  renderer_->SetPlaybackRate(2.0);

  // We push 2880 frames of audio.
  // WSOLA needs at least 2399 frames of lookahead at 48kHz.
  // Render 1: consumes 960 frames. Remaining: 2400 (search index -239 + 2399 =
  // 2160 <= 2400). Succeeds. Render 2: consumes 960 frames. Remaining: 1920
  // (search index 241 + 2399 = 2640 <= 1920 is false). Succeeds (copies from
  // completed). Render 3: underflows (remaining < 2399, no completed frames
  // left). Returns 0.
  auto segment = GenerateSegment(params, /*frames=*/2880);
  ASSERT_TRUE(queue_->Push(segment));

  auto dest = media::AudioBus::Create(params);
  int frames_rendered = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());

  EXPECT_EQ(frames_rendered, 480);

  auto dest2 = media::AudioBus::Create(params);
  int frames_rendered_2 = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest2.get());

  EXPECT_EQ(frames_rendered_2, 480);

  auto dest3 = media::AudioBus::Create(params);
  int frames_rendered_3 = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest3.get());

  EXPECT_EQ(frames_rendered_3, 0);
  EXPECT_TRUE(dest3->AreFramesZero());
}

TEST_F(ReadAloudAudioRendererTest, RenderTimeStretchingSlower) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));

  // Set playback rate to 0.5x (half speed).
  renderer_->SetPlaybackRate(0.5);

  // We push 2400 frames of audio.
  // WSOLA needs at least 2399 frames of lookahead at 48kHz.
  // At 0.5x, search index remains negative for 5 renders, meaning no frames
  // are removed.
  // Render 1 to 5 will succeed (480 frames).
  // Render 6 will underflow.
  auto segment = GenerateSegment(params, /*frames=*/2400);
  ASSERT_TRUE(queue_->Push(segment));

  for (int i = 0; i < 5; ++i) {
    auto dest = media::AudioBus::Create(params);
    int frames_rendered = renderer_->Render(
        /*delay=*/base::TimeDelta(),
        /*delay_timestamp=*/base::TimeTicks::Now(),
        /*glitch_info=*/media::AudioGlitchInfo(), dest.get());
    EXPECT_EQ(frames_rendered, 480) << "Failed at render index " << i;
    EXPECT_FALSE(dest->AreFramesZero())
        << "Expected audio at render index " << i;
  }

  auto dest_fail = media::AudioBus::Create(params);
  int frames_rendered_fail = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest_fail.get());

  EXPECT_EQ(frames_rendered_fail, 0);
  EXPECT_TRUE(dest_fail->AreFramesZero());
}

TEST_F(ReadAloudAudioRendererTest, FlushClearsInternalAlgorithmBuffers) {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));

  // Push audio segment and render to populate algorithm queue.
  auto segment = GenerateSegment(params, /*frames=*/2880);
  ASSERT_TRUE(queue_->Push(segment));

  auto dest = media::AudioBus::Create(params);
  int frames_rendered = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest.get());
  EXPECT_EQ(frames_rendered, 480);

  // Calling Flush should empty the algorithm's internal queue.
  renderer_->Flush();

  // Subsequent render call should return 0 frames rendered and zero out buffer.
  auto dest_after_flush = media::AudioBus::Create(params);
  int frames_after_flush = renderer_->Render(
      /*delay=*/base::TimeDelta(),
      /*delay_timestamp=*/base::TimeTicks::Now(),
      /*glitch_info=*/media::AudioGlitchInfo(), dest_after_flush.get());

  EXPECT_EQ(frames_after_flush, 0);
  EXPECT_TRUE(dest_after_flush->AreFramesZero());
}

TEST_F(ReadAloudAudioRendererTest, GetMediaTimeIsZeroBeforeInitialize) {
  EXPECT_EQ(renderer_->GetMediaTime(), base::TimeDelta());
}

TEST_F(ReadAloudAudioRendererTest, RenderAdvancesMediaTimeAsAudioPlaysOut) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  ASSERT_TRUE(queue_->Push(GenerateSegment(params, /*frames=*/1440)));

  // With no output delay, each Render() hands the previous 10 ms buffer to the
  // output, so the audible media time trails the written audio by one buffer.
  RenderOnce(params);
  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(0));
  RenderOnce(params);
  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(10));
  RenderOnce(params);
  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(20));
}

TEST_F(ReadAloudAudioRendererTest, RenderScalesMediaTimeByPlaybackRate) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  renderer_->SetPlaybackRate(2.0);
  ASSERT_TRUE(queue_->Push(GenerateSegment(params, /*frames=*/2880)));

  RenderOnce(params);
  RenderOnce(params);

  // At 2x, the 10 ms output buffer that has played out carried 20 ms of media.
  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(20));
}

TEST_F(ReadAloudAudioRendererTest, RenderSubtractsOutputDelayFromMediaTime) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  ASSERT_TRUE(queue_->Push(GenerateSegment(params, /*frames=*/960)));

  RenderOnce(params, /*delay=*/base::Milliseconds(5));
  RenderOnce(params, /*delay=*/base::Milliseconds(5));

  // 5 ms of the first buffer is still in the output pipeline.
  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(5));
}

TEST_F(ReadAloudAudioRendererTest, RenderTreatsNegativeDelayAsZero) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  ASSERT_TRUE(queue_->Push(GenerateSegment(params, /*frames=*/960)));

  // AudioClock::WroteAudio() CHECKs on a negative delay.
  RenderOnce(params, /*delay=*/base::Milliseconds(-5));
  RenderOnce(params, /*delay=*/base::Milliseconds(-5));

  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(10));
}

TEST_F(ReadAloudAudioRendererTest, RenderCapsOutputDelay) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  // With a delay of N buffers, the clock stays at zero for the first N + 1
  // renders and then advances by one buffer per render. So after
  // `cap_buffers + 2` renders, a delay capped at `kMaxAcceptableDelay` leaves
  // exactly one buffer played out, while an uncapped (larger) one leaves none.
  const base::TimeDelta buffer_duration = params.GetBufferDuration();
  const int64_t cap_buffers =
      ReadAloudAudioRenderer::kMaxAcceptableDelay.IntDiv(buffer_duration);
  const int64_t renders = cap_buffers + 2;
  ASSERT_TRUE(queue_->Push(GenerateSegment(
      params,
      /*frames=*/base::checked_cast<int>((renders + 1) *
                                         params.frames_per_buffer()))));

  for (int64_t i = 0; i < renders; ++i) {
    RenderOnce(params,
               /*delay=*/ReadAloudAudioRenderer::kMaxAcceptableDelay * 2);
  }

  EXPECT_EQ(renderer_->GetMediaTime(), buffer_duration);
}

TEST_F(ReadAloudAudioRendererTest, FlushRestartsMediaTimeAtZero) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  ASSERT_TRUE(queue_->Push(GenerateSegment(params, /*frames=*/1440)));
  RenderOnce(params);
  RenderOnce(params);
  ASSERT_EQ(renderer_->GetMediaTime(), base::Milliseconds(10));

  renderer_->Flush();

  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(0));
}

TEST_F(ReadAloudAudioRendererTest, MediaTimeAfterFlushCountsOnlyNewAudio) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  ASSERT_TRUE(queue_->Push(GenerateSegment(params, /*frames=*/1440)));
  RenderOnce(params);
  RenderOnce(params);
  renderer_->Flush();

  ASSERT_TRUE(queue_->Push(GenerateSegment(params, /*frames=*/960)));
  RenderOnce(params);
  RenderOnce(params);

  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(10));
}

TEST_F(ReadAloudAudioRendererTest, DispatchesEachWordExactlyWhenAudible) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  RecordBoundaries();
  const WordTiming first = MakeWord(base::TimeDelta(),
                                    /*start_character_offset=*/0,
                                    /*end_character_offset=*/5);
  const WordTiming second = MakeWord(base::Milliseconds(10),
                                     /*start_character_offset=*/6,
                                     /*end_character_offset=*/11);
  ASSERT_TRUE(queue_->Push(
      GenerateSegmentWithTimings(params, /*frames=*/960, {first, second})));

  RenderOnce(params);

  // The exact timings below pin that a word is dispatched when it becomes
  // audible: never early, and not late either.

  // The first word starts at media time 0, which is audible right away. A
  // zero fast-forward runs the posted pump without advancing time.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_THAT(boundaries_,
              testing::ElementsAre(BoundaryFor(first, base::TimeDelta())));

  // The second word starts 10 ms in. It must not be reported early...
  task_environment_.FastForwardBy(base::Milliseconds(9));
  EXPECT_THAT(boundaries_,
              testing::ElementsAre(BoundaryFor(first, base::TimeDelta())));

  // ...and is reported on time by the scheduled pump alone, without waiting
  // for another Render().
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(boundaries_, testing::ElementsAre(
                               BoundaryFor(first, base::TimeDelta()),
                               BoundaryFor(second, base::Milliseconds(10))));
}

TEST_F(ReadAloudAudioRendererTest, DispatchesAllWordsDueInTheSamePumpRun) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  RecordBoundaries();
  const WordTiming first = MakeWord(base::TimeDelta(),
                                    /*start_character_offset=*/0,
                                    /*end_character_offset=*/2);
  const WordTiming second = MakeWord(base::TimeDelta(),
                                     /*start_character_offset=*/3,
                                     /*end_character_offset=*/5);
  ASSERT_TRUE(queue_->Push(
      GenerateSegmentWithTimings(params, /*frames=*/960, {first, second})));

  RenderOnce(params);
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_THAT(boundaries_,
              testing::ElementsAre(BoundaryFor(first, base::TimeDelta()),
                                   BoundaryFor(second, base::TimeDelta())));
}

TEST_F(ReadAloudAudioRendererTest, DispatchesWordBoundariesAtPlaybackRate) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  RecordBoundaries();
  renderer_->SetPlaybackRate(2.0);
  const WordTiming word = MakeWord(base::Milliseconds(10),
                                   /*start_character_offset=*/2,
                                   /*end_character_offset=*/6);
  ASSERT_TRUE(queue_->Push(
      GenerateSegmentWithTimings(params, /*frames=*/2880, {word})));

  RenderOnce(params);

  // At 2x, the word 10 ms into the media timeline is audible after 5 ms.
  task_environment_.FastForwardBy(base::Milliseconds(4));
  EXPECT_THAT(boundaries_, testing::IsEmpty());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(boundaries_,
              testing::ElementsAre(BoundaryFor(word, base::Milliseconds(10))));
}

TEST_F(ReadAloudAudioRendererTest,
       DispatchesWordInLaterSegmentAfterDurationEnqueuedBeforeIt) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  RecordBoundaries();
  const WordTiming word = MakeWord(base::Milliseconds(1),
                                   /*start_character_offset=*/7,
                                   /*end_character_offset=*/9);
  ASSERT_TRUE(queue_->Push(
      GenerateSegmentWithTimings(params, /*frames=*/480, /*timings=*/{})));
  ASSERT_TRUE(
      queue_->Push(GenerateSegmentWithTimings(params, /*frames=*/480, {word})));

  RenderOnce(params);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_THAT(boundaries_, testing::IsEmpty());

  // 1 ms into the second segment is 11 ms into the media timeline.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(boundaries_,
              testing::ElementsAre(BoundaryFor(word, base::Milliseconds(11))));
}

TEST_F(ReadAloudAudioRendererTest,
       DoesNotDispatchBoundariesWhileRenderStalled) {
  // Pausing playback simply stops Render() from being called, which freezes
  // the clock anchor. Extrapolating from a frozen anchor would run through
  // every remaining word while nothing is audible.
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  RecordBoundaries();
  // The word only becomes due after the anchor has gone stale.
  const base::TimeDelta word_start = WordBoundaryQueue::kMaxExtrapolation * 2;
  ASSERT_TRUE(queue_->Push(GenerateSegmentWithTimings(
      params, FramesFor(params, word_start * 2),
      {MakeWord(word_start, /*start_character_offset=*/3,
                /*end_character_offset=*/8)})));

  // A single Render() anchors the clock at media time 0, then nothing more
  // arrives, as if playback were paused right afterwards.
  RenderOnce(params);
  task_environment_.FastForwardBy(word_start * 10);

  EXPECT_THAT(boundaries_, testing::IsEmpty());
  // The pump stops rescheduling itself instead of polling while stalled.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(ReadAloudAudioRendererTest, ResumesDispatchWhenRenderResumesAfterStall) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  RecordBoundaries();
  const base::TimeDelta buffer_duration = params.GetBufferDuration();
  const base::TimeDelta word_start = WordBoundaryQueue::kMaxExtrapolation * 2;
  const WordTiming word = MakeWord(word_start, /*start_character_offset=*/3,
                                   /*end_character_offset=*/8);
  ASSERT_TRUE(queue_->Push(GenerateSegmentWithTimings(
      params, FramesFor(params, word_start * 2), {word})));

  // Stall long enough for the pump to give up on the anchor.
  RenderOnce(params);
  task_environment_.FastForwardBy(word_start * 2);
  ASSERT_THAT(boundaries_, testing::IsEmpty());

  // Resume rendering one buffer at a time until the word is one buffer away
  // from being audible. The first render after resuming makes one buffer
  // audible, and each later one another buffer.
  const int64_t resume_renders = word_start.IntDiv(buffer_duration) - 1;
  RenderOnce(params);
  for (int64_t i = 1; i < resume_renders; ++i) {
    task_environment_.FastForwardBy(buffer_duration);
    RenderOnce(params);
  }
  ASSERT_EQ(renderer_->GetMediaTime(), word_start - buffer_duration);
  EXPECT_THAT(boundaries_, testing::IsEmpty());

  task_environment_.FastForwardBy(buffer_duration);
  EXPECT_THAT(boundaries_, testing::ElementsAre(BoundaryFor(word, word_start)));
}

TEST_F(ReadAloudAudioRendererTest, FlushDropsBoundariesQueuedBeforeFlush) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  RecordBoundaries();
  const WordTiming word_before_flush =
      MakeWord(base::Milliseconds(5), /*start_character_offset=*/0,
               /*end_character_offset=*/4);
  const WordTiming word_after_flush =
      MakeWord(base::Milliseconds(1), /*start_character_offset=*/8,
               /*end_character_offset=*/12);
  ASSERT_TRUE(queue_->Push(
      GenerateSegmentWithTimings(params, /*frames=*/960, {word_before_flush})));
  RenderOnce(params);

  renderer_->Flush();
  // Audio queued after the flush must dispatch only its own word.
  ASSERT_TRUE(queue_->Push(
      GenerateSegmentWithTimings(params, /*frames=*/960, {word_after_flush})));
  RenderOnce(params);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_THAT(boundaries_, testing::ElementsAre(BoundaryFor(
                               word_after_flush, base::Milliseconds(1))));
}

TEST_F(ReadAloudAudioRendererTest,
       BufferlessSegmentDoesNotShiftLaterBoundaries) {
  const media::AudioParameters params = MakeParams();
  ASSERT_TRUE(renderer_->Initialize(params, queue_.get()));
  RecordBoundaries();
  const WordTiming word = MakeWord(base::Milliseconds(1),
                                   /*start_character_offset=*/4,
                                   /*end_character_offset=*/7);
  // A segment with a duration but no audio buffer is never played, so it
  // takes up no media time.
  ASSERT_TRUE(queue_->Push(
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10))));
  ASSERT_TRUE(
      queue_->Push(GenerateSegmentWithTimings(params, /*frames=*/960, {word})));

  RenderOnce(params);
  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_THAT(boundaries_,
              testing::ElementsAre(BoundaryFor(word, base::Milliseconds(1))));
}

}  // namespace readaloud
