// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoder/read_aloud_decoder_sequencer.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/services/readaloud/audio_segment_queue.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "chrome/services/readaloud/decoder/opus_decoder_helper.h"
#include "chrome/services/readaloud/prefetch/prefetch_manager.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

namespace {

class FakeOpusDecoderHelper : public OpusDecoderHelper {
 public:
  FakeOpusDecoderHelper() = default;
  ~FakeOpusDecoderHelper() override = default;

  void DecodeAndSlice(
      scoped_refptr<media::DecoderBuffer> container_buffer,
      const std::vector<DecodedAudioSegment::WordTiming>& timings,
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
        audio_queue_(std::make_unique<AudioSegmentQueue>()),
        sequencer_(&prefetch_manager_, &fake_decoder_, audio_queue_.get()) {}

  void SetUpTimeline(size_t chunk_count) {
    std::vector<read_aloud::mojom::TextSegmentPtr> segments;
    for (size_t i = 0; i < chunk_count; ++i) {
      read_aloud::mojom::TextSegmentPtr seg =
          read_aloud::mojom::TextSegment::New();
      seg->segment_index = i;
      seg->text = u"Sentence.";
      segments.push_back(std::move(seg));
    }
    prefetch_manager_.SetTextContent(segments);
    ASSERT_EQ(prefetch_manager_.GetTimelineChunkCount(), chunk_count);
  }

  void InsertCachedSegment(uint32_t chunk_index,
                           scoped_refptr<media::DecoderBuffer> opus_buffer) {
    prefetch_manager_.InsertCachedSegment(chunk_index, std::move(opus_buffer),
                                          /*timings=*/{});
  }

  scoped_refptr<media::DecoderBuffer> CreateDummyBuffer() {
    return media::DecoderBuffer::CopyFrom(std::vector<uint8_t>{0x4f, 0x67});
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  PrefetchManager prefetch_manager_;
  FakeOpusDecoderHelper fake_decoder_;
  std::unique_ptr<AudioSegmentQueue> audio_queue_;
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
  InsertCachedSegment(/*chunk_index=*/0, /*opus_buffer=*/nullptr);
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

}  // namespace readaloud
