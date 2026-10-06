// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoder/read_aloud_decoder_sequencer.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/services/readaloud/audio_segment_queue.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "chrome/services/readaloud/decoder/opus_decoder_helper.h"
#include "chrome/services/readaloud/prefetch/prefetch_manager.h"
#include "chrome/services/readaloud/timeline/playback_timeline.h"
#include "chrome/services/readaloud/word_timing.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

namespace {

class FakeOpusDecoderHelper : public OpusDecoderHelper {
 public:
  FakeOpusDecoderHelper() = default;
  ~FakeOpusDecoderHelper() override = default;

  void DecodeAndSlice(scoped_refptr<media::DecoderBuffer> container_buffer,
                      const std::vector<WordTiming>& timings,
                      DecodeCallback callback) override {
    last_callback_ = std::move(callback);
    decode_call_count_++;
  }

  bool HasPendingCallback() const { return !last_callback_.is_null(); }

  void DeliverDecodedSegments(
      std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
    if (last_callback_) {
      std::move(last_callback_).Run(std::move(segments));
    }
  }

  size_t decode_call_count() const { return decode_call_count_; }

 private:
  DecodeCallback last_callback_;
  size_t decode_call_count_ = 0;
};

}  // namespace

class ReadAloudDecoderSequencerTest : public testing::Test {
 public:
  ReadAloudDecoderSequencerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        prefetch_manager_(&timeline_),
        audio_queue_(std::make_unique<AudioSegmentQueue>()),
        sequencer_(&prefetch_manager_, &fake_decoder_, audio_queue_.get()) {
    // `sequencer_` is destroyed before `pump_reports_`, so the callback never
    // outlives the vector it writes to.
    sequencer_.SetPumpStatusCallback(base::BindLambdaForTesting(
        [this](ReadAloudDecoderSequencer::PumpStatus status) {
          pump_reports_.push_back(status);
        }));
  }

  void SetUpTimeline(size_t chunk_count) {
    prefetch_manager_.ResetSession();
    timeline_.Clear();
    std::vector<read_aloud::mojom::TextSegmentPtr> segments;
    segments.reserve(chunk_count);
    for (size_t i = 0; i < chunk_count; ++i) {
      read_aloud::mojom::TextSegmentPtr seg =
          read_aloud::mojom::TextSegment::New();
      seg->segment_index = i;
      seg->text = u"Sentence.";
      segments.push_back(std::move(seg));
    }
    timeline_.SetTextContent(std::move(segments));
    EXPECT_EQ(prefetch_manager_.GetTimelineChunkCount(), chunk_count);
  }

  void InsertCachedSegment(
      uint32_t chunk_index,
      scoped_refptr<media::DecoderBuffer> opus_buffer,
      SynthesisResultStatus status = SynthesisResultStatus::kSuccess) {
    prefetch_manager_.InsertCachedSegment(chunk_index, std::move(opus_buffer),
                                          /*timings=*/{}, status);
  }

  scoped_refptr<media::DecoderBuffer> CreateDummyBuffer() {
    return media::DecoderBuffer::CopyFrom(std::vector<uint8_t>{0x4f, 0x67});
  }

  scoped_refptr<DecodedAudioSegment> CreateSegmentWithWordTiming(
      const WordTiming& timing) {
    scoped_refptr<media::AudioBuffer> audio_buffer =
        media::AudioBuffer::CreateEmptyBuffer(
            media::ChannelLayout::CHANNEL_LAYOUT_MONO, /*channel_count=*/1,
            /*sample_rate=*/48000, /*frame_count=*/48000, base::TimeDelta());
    return base::MakeRefCounted<DecodedAudioSegment>(
        std::move(audio_buffer), std::vector<WordTiming>{timing});
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  PlaybackTimeline timeline_;
  PrefetchManager prefetch_manager_;
  FakeOpusDecoderHelper fake_decoder_;
  std::unique_ptr<AudioSegmentQueue> audio_queue_;
  // Every PumpStatus reported by `sequencer_`, in order.
  std::vector<ReadAloudDecoderSequencer::PumpStatus> pump_reports_;
  ReadAloudDecoderSequencer sequencer_;
};

TEST_F(ReadAloudDecoderSequencerTest,
       ReplenishBufferInOrderSequentialExecution) {
  SetUpTimeline(/*chunk_count=*/2);

  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 0u);
  EXPECT_FALSE(sequencer_.is_decoding());

  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());
  InsertCachedSegment(/*chunk_index=*/1, CreateDummyBuffer());

  // First replenish triggers decoding of chunk 0.
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 0u);
  EXPECT_TRUE(fake_decoder_.HasPendingCallback());

  // Simulate completion of decoding for chunk 0.
  scoped_refptr<DecodedAudioSegment> segment0 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2));
  fake_decoder_.DeliverDecodedSegments({segment0});

  // Chunk 0 is pushed to audio_queue, cursor advances to 1,
  // and sequencer automatically starts decoding chunk 1.
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 1u);
  EXPECT_EQ(audio_queue_->size(), 1u);
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_TRUE(fake_decoder_.HasPendingCallback());

  // Simulate completion of decoding for chunk 1.
  scoped_refptr<DecodedAudioSegment> segment1 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(3));
  fake_decoder_.DeliverDecodedSegments({segment1});

  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 2u);
  EXPECT_EQ(audio_queue_->size(), 2u);
  EXPECT_FALSE(sequencer_.is_decoding());
  EXPECT_FALSE(fake_decoder_.HasPendingCallback());
}

TEST_F(ReadAloudDecoderSequencerTest,
       OutOfOrderSynthesisResponseWaitsForInOrderChunk) {
  SetUpTimeline(/*chunk_count=*/2);

  // Cache chunk 1 first (out-of-order response).
  InsertCachedSegment(/*chunk_index=*/1, CreateDummyBuffer());

  // ReplenishBuffer checks chunk 0. Since chunk 0 is missing, chunk 1 must NOT
  // be decoded.
  sequencer_.ReplenishBuffer();
  EXPECT_FALSE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 0u);
  EXPECT_EQ(audio_queue_->size(), 0u);
  EXPECT_FALSE(fake_decoder_.HasPendingCallback());

  // Now cache chunk 0.
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  // Trigger replenish: chunk 0 should now begin decoding.
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 0u);
  EXPECT_TRUE(fake_decoder_.HasPendingCallback());

  // Complete chunk 0 decode.
  scoped_refptr<DecodedAudioSegment> segment0 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2));
  fake_decoder_.DeliverDecodedSegments({segment0});

  // Cursor advances to 1 and immediately initiates decode for cached chunk 1.
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 1u);
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(audio_queue_->size(), 1u);
  EXPECT_TRUE(fake_decoder_.HasPendingCallback());
}

TEST_F(ReadAloudDecoderSequencerTest,
       ReplenishBufferThrottlesWhenAudioQueueFull) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  // Fill audio queue up to kMaxDecodedAudioDuration watermark (50s).
  scoped_refptr<DecodedAudioSegment> full_segment =
      base::MakeRefCounted<DecodedAudioSegment>(kMaxDecodedAudioDuration);
  EXPECT_TRUE(audio_queue_->Push(full_segment));
  EXPECT_GE(audio_queue_->GetBufferedDuration(), kMaxDecodedAudioDuration);

  // Sequencer must halt because buffered duration is at watermark.
  sequencer_.ReplenishBuffer();
  EXPECT_FALSE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 0u);
  EXPECT_FALSE(fake_decoder_.HasPendingCallback());
}

TEST_F(ReadAloudDecoderSequencerTest,
       ReplenishBufferResumesAfterAudioQueueDrains) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  // Fill queue to watermark.
  scoped_refptr<DecodedAudioSegment> full_segment =
      base::MakeRefCounted<DecodedAudioSegment>(kMaxDecodedAudioDuration);
  EXPECT_TRUE(audio_queue_->Push(full_segment));

  sequencer_.ReplenishBuffer();
  EXPECT_FALSE(sequencer_.is_decoding());

  // Drain the queue.
  scoped_refptr<DecodedAudioSegment> popped = audio_queue_->Pop();
  ASSERT_NE(popped, nullptr);
  EXPECT_EQ(audio_queue_->GetBufferedDuration(), base::TimeDelta());

  // ReplenishBuffer should now proceed with decoding chunk 0.
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 0u);
  EXPECT_TRUE(fake_decoder_.HasPendingCallback());
}

TEST_F(ReadAloudDecoderSequencerTest,
       ConcurrentReplenishCallsDoNotDuplicateDecode) {
  SetUpTimeline(/*chunk_count=*/1);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  // Initial replenish begins decoding chunk 0.
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(fake_decoder_.decode_call_count(), 1u);

  // Subsequent calls while is_decoding is true are no-ops.
  sequencer_.ReplenishBuffer();
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(fake_decoder_.decode_call_count(), 1u);
}

TEST_F(ReadAloudDecoderSequencerTest, StaleSequenceIdDecodesAreDiscarded) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());

  // Simulate a session reset or cache clear in prefetch_manager, advancing
  // sequence ID.
  prefetch_manager_.ResetSession();

  // Deliver decoded segments for the old sequence.
  scoped_refptr<DecodedAudioSegment> segment0 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2));
  fake_decoder_.DeliverDecodedSegments({segment0});

  // Stale segments must not be pushed to the queue and cursor should not
  // advance.
  EXPECT_EQ(audio_queue_->size(), 0u);
  EXPECT_FALSE(sequencer_.is_decoding());
}

TEST_F(ReadAloudDecoderSequencerTest,
       ResetCancelsInFlightDecodesAndResetsCursor) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());

  sequencer_.Reset();
  EXPECT_FALSE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 0u);

  // Delivering stale callback after Reset must have no effect.
  scoped_refptr<DecodedAudioSegment> segment0 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2));
  fake_decoder_.DeliverDecodedSegments({segment0});
  EXPECT_EQ(audio_queue_->size(), 0u);
}

TEST_F(ReadAloudDecoderSequencerTest, HandlesNullCachedSegmentWithoutStalling) {
  SetUpTimeline(/*chunk_count=*/2);
  // Insert null audio buffer for chunk 0 (simulating failed synthesis response)
  InsertCachedSegment(/*chunk_index=*/0, /*opus_buffer=*/nullptr,
                      SynthesisResultStatus::kSynthesisError);
  InsertCachedSegment(/*chunk_index=*/1, CreateDummyBuffer());

  // 1. ReplenishBuffer skips null chunk 0 and begins decoding chunk 1
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 1u);

  // 2. Deliver decoded PCM segments for chunk 1
  scoped_refptr<DecodedAudioSegment> segment1 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2));
  fake_decoder_.DeliverDecodedSegments({segment1});

  // 3. Sequencer finishes chunk 1, advances cursor to 2u, and pushes to queue
  EXPECT_FALSE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 2u);
  EXPECT_EQ(audio_queue_->size(), 1u);
}

TEST_F(ReadAloudDecoderSequencerTest, SkipsCorruptDataSegmentWithoutStalling) {
  SetUpTimeline(/*chunk_count=*/2);
  scoped_refptr<media::DecoderBuffer> corrupt_buffer =
      media::DecoderBuffer::CopyFrom(std::vector<uint8_t>{0xff, 0xff});
  InsertCachedSegment(/*chunk_index=*/0, corrupt_buffer,
                      SynthesisResultStatus::kCorruptData);
  InsertCachedSegment(/*chunk_index=*/1, CreateDummyBuffer());

  // ReplenishBuffer logs warning, skips corrupt chunk 0, and begins decoding
  // chunk 1
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 1u);
}

TEST_F(ReadAloudDecoderSequencerTest,
       ReplenishBufferLookaheadReplenishesOnChunkFailure) {
  SetUpTimeline(/*chunk_count=*/6);
  std::vector<uint32_t> dispatched_indices;
  prefetch_manager_.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text,
         read_aloud::mojom::Speaker speaker) { out->push_back(chunk_index); },
      &dispatched_indices));

  // Insert failed synthesis response for chunk 0 and valid buffer for chunk 1
  InsertCachedSegment(/*chunk_index=*/0, /*opus_buffer=*/nullptr,
                      SynthesisResultStatus::kSynthesisError);
  InsertCachedSegment(/*chunk_index=*/1, CreateDummyBuffer());

  sequencer_.ReplenishBuffer();
  // Cursor advances to 1u, decoding starts on chunk 1, and lookahead extends to
  // chunk 5 (queued in pending_requests_)
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 1u);
  EXPECT_TRUE(sequencer_.is_decoding());

  // Completing chunk 2 frees an in-flight slot and dispatches chunk 5 from
  // pending queue
  uint64_t seq_id = prefetch_manager_.GetCurrentSequenceId();
  prefetch_manager_.OnSynthesisResponse(seq_id, 2, CreateDummyBuffer(), {});
  EXPECT_FALSE(dispatched_indices.empty());
  EXPECT_EQ(dispatched_indices.back(), 5u);
}

TEST_F(ReadAloudDecoderSequencerTest, ReentrancyGuardPreventsRecursiveReplenish) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  // Trigger ReplenishBuffer
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());

  // Additional call to ReplenishBuffer while is_decoding is true should return early
  sequencer_.ReplenishBuffer();
  EXPECT_EQ(fake_decoder_.decode_call_count(), 1u);
}

TEST_F(ReadAloudDecoderSequencerTest, PumpingWithEmptyQueueReportsStarved) {
  SetUpTimeline(/*chunk_count=*/2);

  // Nothing has been synthesized yet, so the renderer queue is dry.
  sequencer_.StartPumping();

  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kStarved));
}

TEST_F(ReadAloudDecoderSequencerTest, DecodedAudioReportsFlowing) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  sequencer_.StartPumping();
  fake_decoder_.DeliverDecodedSegments(
      {base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2))});

  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kStarved,
                           ReadAloudDecoderSequencer::PumpStatus::kFlowing));
}

TEST_F(ReadAloudDecoderSequencerTest, ReplenishWhileNotPumpingReportsNothing) {
  SetUpTimeline(/*chunk_count=*/2);

  // Replenish cycles triggered outside of active playback must stay silent.
  sequencer_.ReplenishBuffer();

  EXPECT_THAT(pump_reports_, testing::IsEmpty());
}

TEST_F(ReadAloudDecoderSequencerTest, TimelineWithNoUsableAudioReportsFailed) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, /*opus_buffer=*/nullptr,
                      SynthesisResultStatus::kSynthesisError);
  InsertCachedSegment(/*chunk_index=*/1, /*opus_buffer=*/nullptr,
                      SynthesisResultStatus::kSynthesisError);

  // Every chunk is skipped, so the timeline is consumed without ever
  // producing a single audio segment.
  sequencer_.StartPumping();

  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kFailed));
}

TEST_F(ReadAloudDecoderSequencerTest,
       TrailingChunkFailureAfterAudioReportsDrained) {
  SetUpTimeline(/*chunk_count=*/2);
  // Chunk 0 decodes fine, the last chunk is unusable.
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());
  InsertCachedSegment(/*chunk_index=*/1, /*opus_buffer=*/nullptr,
                      SynthesisResultStatus::kSynthesisError);

  sequencer_.StartPumping();
  fake_decoder_.DeliverDecodedSegments(
      {base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2))});
  ASSERT_TRUE(audio_queue_->Pop());
  sequencer_.ReplenishBuffer();

  // Some audio was produced before the bad chunk was skipped, so exhausting
  // the timeline is a normal end of document rather than a total failure.
  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kStarved,
                           ReadAloudDecoderSequencer::PumpStatus::kFlowing,
                           ReadAloudDecoderSequencer::PumpStatus::kDrained));
}

TEST_F(ReadAloudDecoderSequencerTest,
       ExhaustedTimelineWithAudioReportsDrained) {
  SetUpTimeline(/*chunk_count=*/1);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  sequencer_.StartPumping();
  fake_decoder_.DeliverDecodedSegments(
      {base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2))});
  // The renderer consuming the last segment after the timeline is exhausted is
  // a normal end of document, not a failure.
  ASSERT_TRUE(audio_queue_->Pop());
  sequencer_.ReplenishBuffer();

  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kStarved,
                           ReadAloudDecoderSequencer::PumpStatus::kFlowing,
                           ReadAloudDecoderSequencer::PumpStatus::kDrained));
}

TEST_F(ReadAloudDecoderSequencerTest, PumpTicksAfterDrainKeepReportingDrained) {
  SetUpTimeline(/*chunk_count=*/1);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  sequencer_.StartPumping();
  fake_decoder_.DeliverDecodedSegments(
      {base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2))});
  ASSERT_TRUE(audio_queue_->Pop());
  pump_reports_.clear();

  // The sequencer keeps no memory of having already announced the end of the
  // document, so every subsequent tick repeats kDrained. Collapsing those into
  // a single client notification is the controller's dedupe responsibility.
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_THAT(
      pump_reports_,
      testing::AllOf(
          testing::SizeIs(testing::Gt(1u)),
          testing::Each(ReadAloudDecoderSequencer::PumpStatus::kDrained)));
}

TEST_F(ReadAloudDecoderSequencerTest,
       ResetClearsProducedAudioSoTotalFailureReportsFailed) {
  // First document plays to completion and produces audio.
  SetUpTimeline(/*chunk_count=*/1);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());
  sequencer_.StartPumping();
  fake_decoder_.DeliverDecodedSegments(
      {base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2))});
  ASSERT_TRUE(audio_queue_->Pop());

  // Second document, whose only chunk fails, must not inherit the first
  // document's audio and be mistaken for a normal end of document.
  sequencer_.Reset();
  SetUpTimeline(/*chunk_count=*/1);
  InsertCachedSegment(/*chunk_index=*/0, /*opus_buffer=*/nullptr,
                      SynthesisResultStatus::kSynthesisError);
  pump_reports_.clear();

  sequencer_.StartPumping();

  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kFailed));
}

TEST_F(ReadAloudDecoderSequencerTest,
       SetNextChunkToDecodeCancelsInFlightDecode) {
  SetUpTimeline(/*chunk_count=*/3);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());
  InsertCachedSegment(/*chunk_index=*/2, CreateDummyBuffer());

  // Start decoding chunk 0.
  sequencer_.ReplenishBuffer();
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 0u);
  EXPECT_EQ(fake_decoder_.decode_call_count(), 1u);

  // Seek to chunk 2 while chunk 0 decode is still in flight.
  // SetNextChunkToDecode must invalidate the in-flight decode callback for
  // chunk 0 and immediately start decoding chunk 2.
  sequencer_.SetNextChunkToDecode(/*chunk_index=*/2,
                                  /*min_global_char_offset=*/20);
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 2u);
  EXPECT_EQ(sequencer_.min_global_char_offset(), 20u);
  EXPECT_EQ(fake_decoder_.decode_call_count(), 2u);

  // Complete chunk 2 decode.
  scoped_refptr<DecodedAudioSegment> segment2 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2));
  fake_decoder_.DeliverDecodedSegments({segment2});
  EXPECT_FALSE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 3u);
  EXPECT_EQ(sequencer_.min_global_char_offset(), 0u);
  EXPECT_EQ(audio_queue_->size(), 1u);
}

TEST_F(ReadAloudDecoderSequencerTest,
       SetNextChunkToDecodeSkipsWordsPrecedingCharOffset) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());
  InsertCachedSegment(/*chunk_index=*/1, CreateDummyBuffer());

  // Seek to chunk 0 with min_global_char_offset = 6 (e.g. second word in
  // "Hello world.").
  sequencer_.SetNextChunkToDecode(/*chunk_index=*/0,
                                  /*min_global_char_offset=*/6);
  EXPECT_TRUE(sequencer_.is_decoding());
  EXPECT_EQ(sequencer_.min_global_char_offset(), 6u);

  // Deliver two word segments for chunk 0:
  // Word 0: [0, 5) -> precedes 6, must be skipped.
  // Word 1: [6, 12) -> starts at 6, must be pushed.
  WordTiming word0{.start_time = base::Seconds(0),
                   .end_time = base::Seconds(1),
                   .start_character_offset = 0,
                   .end_character_offset = 5};
  WordTiming word1{.start_time = base::Seconds(1),
                   .end_time = base::Seconds(2),
                   .start_character_offset = 6,
                   .end_character_offset = 12};
  fake_decoder_.DeliverDecodedSegments(
      {CreateSegmentWithWordTiming(word0), CreateSegmentWithWordTiming(word1)});

  // Only seg_word1 should have been pushed to audio_queue_, and
  // min_global_char_offset_ should reset to 0 for subsequent chunks.
  EXPECT_EQ(audio_queue_->size(), 1u);
  EXPECT_EQ(sequencer_.min_global_char_offset(), 0u);
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 1u);
  EXPECT_TRUE(sequencer_.is_decoding());

  scoped_refptr<DecodedAudioSegment> popped = audio_queue_->Pop();
  ASSERT_NE(popped, nullptr);
  EXPECT_THAT(popped->word_timings(), testing::ElementsAre(word1));

  // Deliver chunk 1 word segment with [12, 18) -> not skipped.
  WordTiming word2{.start_time = base::Seconds(0),
                   .end_time = base::Seconds(1),
                   .start_character_offset = 12,
                   .end_character_offset = 18};
  fake_decoder_.DeliverDecodedSegments({CreateSegmentWithWordTiming(word2)});
  scoped_refptr<DecodedAudioSegment> popped2 = audio_queue_->Pop();
  ASSERT_NE(popped2, nullptr);
  EXPECT_THAT(popped2->word_timings(), testing::ElementsAre(word2));
  EXPECT_EQ(audio_queue_->Pop(), nullptr);
}

TEST_F(ReadAloudDecoderSequencerTest,
       SetNextChunkToDecodePreservesZeroLengthWordTimingAtExactMinOffset) {
  SetUpTimeline(/*chunk_count=*/1);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  sequencer_.SetNextChunkToDecode(/*chunk_index=*/0,
                                  /*min_global_char_offset=*/5);

  // Zero-length WordTiming anchored at [5, 5) must NOT be skipped when
  // min_global_char_offset is 5.
  WordTiming zero_len_word{.start_time = base::Seconds(0),
                           .end_time = base::Seconds(1),
                           .start_character_offset = 5,
                           .end_character_offset = 5};
  fake_decoder_.DeliverDecodedSegments(
      {CreateSegmentWithWordTiming(zero_len_word)});
  EXPECT_EQ(audio_queue_->size(), 1u);
}

TEST_F(ReadAloudDecoderSequencerTest,
       SetNextChunkToDecodeToEndOfTimelineReportsDrainedNotFailed) {
  SetUpTimeline(/*chunk_count=*/2);

  sequencer_.StartPumping();
  pump_reports_.clear();

  // Explicitly seeking to chunk_index == GetTimelineChunkCount() (EOF) before
  // any audio is decoded must report kDrained rather than kFailed.
  sequencer_.SetNextChunkToDecode(/*chunk_index=*/2,
                                  /*min_global_char_offset=*/0);

  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kDrained));
}

TEST_F(ReadAloudDecoderSequencerTest,
       SetNextChunkToDecodeSkippingAllWordsInFinalChunkReportsDrained) {
  SetUpTimeline(/*chunk_count=*/1);
  InsertCachedSegment(/*chunk_index=*/0, CreateDummyBuffer());

  sequencer_.StartPumping();
  // Seek to trailing punctuation offset (11) after the last word [6, 11).
  sequencer_.SetNextChunkToDecode(/*chunk_index=*/0,
                                  /*min_global_char_offset=*/11);
  pump_reports_.clear();

  WordTiming word{.start_time = base::Seconds(0),
                  .end_time = base::Seconds(1),
                  .start_character_offset = 6,
                  .end_character_offset = 11};
  fake_decoder_.DeliverDecodedSegments({CreateSegmentWithWordTiming(word)});

  EXPECT_EQ(audio_queue_->size(), 0u);
  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kDrained));
}

TEST_F(ReadAloudDecoderSequencerTest,
       SeekBackFromEndWhenAllChunksFailReportsFailed) {
  SetUpTimeline(/*chunk_count=*/1);
  InsertCachedSegment(/*chunk_index=*/0, /*opus_buffer=*/nullptr,
                      SynthesisResultStatus::kSynthesisError);

  sequencer_.StartPumping();
  pump_reports_.clear();

  // Seeking to EOF reports kDrained.
  sequencer_.SetNextChunkToDecode(/*chunk_index=*/1,
                                  /*min_global_char_offset=*/0);
  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kDrained));
  pump_reports_.clear();

  // Seeking back to chunk 0 (which failed synthesis) must clear sought_to_end_
  // and report kFailed.
  sequencer_.SetNextChunkToDecode(/*chunk_index=*/0,
                                  /*min_global_char_offset=*/0);
  EXPECT_THAT(
      pump_reports_,
      testing::ElementsAre(ReadAloudDecoderSequencer::PumpStatus::kFailed));
}

TEST_F(ReadAloudDecoderSequencerTest,
       SkippingFailedChunkResetsMinGlobalCharOffset) {
  SetUpTimeline(/*chunk_count=*/2);
  InsertCachedSegment(/*chunk_index=*/0, /*opus_buffer=*/nullptr,
                      SynthesisResultStatus::kSynthesisError);
  InsertCachedSegment(/*chunk_index=*/1, CreateDummyBuffer());

  // Seek into chunk 0 with a non-zero character offset; chunk 0 fails and is
  // skipped, which must reset min_global_char_offset_ to 0 for chunk 1.
  sequencer_.SetNextChunkToDecode(/*chunk_index=*/0,
                                  /*min_global_char_offset=*/15);
  EXPECT_EQ(sequencer_.next_chunk_to_decode(), 1u);
  EXPECT_EQ(sequencer_.min_global_char_offset(), 0u);
  EXPECT_TRUE(sequencer_.is_decoding());
}

}  // namespace readaloud
