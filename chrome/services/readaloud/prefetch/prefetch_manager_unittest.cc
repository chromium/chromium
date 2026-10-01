// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/services/readaloud/timeline/playback_timeline.h"
#include "chrome/services/readaloud/word_timing.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class PrefetchManagerTest : public testing::Test {
 protected:
  void SetTimelineFromStrings(PrefetchManager& manager,
                              std::vector<std::u16string> texts) {
    manager.ResetSession();
    timeline_.Clear();
    std::vector<read_aloud::mojom::TextSegmentPtr> segments;
    segments.reserve(texts.size());
    for (std::u16string& text : texts) {
      auto seg = read_aloud::mojom::TextSegment::New();
      seg->text = std::move(text);
      seg->speaker = read_aloud::mojom::Speaker::kSpeaker1;
      segments.push_back(std::move(seg));
    }
    timeline_.SetTextContent(std::move(segments));
  }

  PlaybackTimeline timeline_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PrefetchManagerTest, DefaultConstructor) {
  PrefetchManager manager(&timeline_);
  EXPECT_FALSE(manager.HasCachedSegment(0));
  EXPECT_EQ(manager.GetCachedSegment(0), nullptr);
  EXPECT_EQ(manager.GetTimelineChunkCount(), 0u);
  EXPECT_EQ(manager.GetCurrentSequenceId(), 0u);
  EXPECT_EQ(manager.GetInflightRequestCount(), 0u);
}

TEST_F(PrefetchManagerTest, InsertAndRetrieveCachedSegment) {
  PrefetchManager manager(&timeline_);
  std::vector<WordTiming> timings = {{.start_time = base::Milliseconds(0),
                                      .end_time = base::Milliseconds(200),
                                      .start_character_offset = 0u,
                                      .end_character_offset = 5u},
                                     {.start_time = base::Milliseconds(200),
                                      .end_time = base::Milliseconds(500),
                                      .start_character_offset = 6u,
                                      .end_character_offset = 11u}};

  manager.InsertCachedSegment(
      0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      timings);

  EXPECT_TRUE(manager.HasCachedSegment(0));
  EXPECT_FALSE(manager.HasCachedSegment(1));

  const CachedCompressedSegment* cached = manager.GetCachedSegment(0);
  ASSERT_NE(cached, nullptr);
  ASSERT_NE(cached->opus_buffer, nullptr);
  EXPECT_EQ(cached->opus_buffer->size(), 4u);
  EXPECT_EQ(*cached->opus_buffer->begin(), 0x4F);
  EXPECT_THAT(cached->timings,
              testing::ElementsAre(
                  testing::FieldsAre(base::Milliseconds(0),
                                     base::Milliseconds(200), 0u, 5u),
                  testing::FieldsAre(base::Milliseconds(200),
                                     base::Milliseconds(500), 6u, 11u)));
}

TEST_F(PrefetchManagerTest, ResetSessionClearsCache) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Hello Chromium."});
  manager.InsertCachedSegment(
      0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  EXPECT_TRUE(manager.HasCachedSegment(0));

  manager.ResetSession();

  EXPECT_FALSE(manager.HasCachedSegment(0));
  EXPECT_EQ(manager.GetCachedSegment(0), nullptr);
}

TEST_F(PrefetchManagerTest, ClearCachePurgesAudioWithoutClearingTimeline) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Hello Chromium."});
  manager.InsertCachedSegment(
      0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  EXPECT_EQ(manager.GetTimelineChunkCount(), 1u);
  EXPECT_TRUE(manager.HasCachedSegment(0));

  manager.ClearCache();

  EXPECT_EQ(manager.GetTimelineChunkCount(), 1u);
  EXPECT_FALSE(manager.HasCachedSegment(0));
}

TEST_F(PrefetchManagerTest, GetCachedSegmentWithUncachedIndexReturnsNull) {
  PrefetchManager manager(&timeline_);
  EXPECT_FALSE(manager.HasCachedSegment(999u));
  EXPECT_EQ(manager.GetCachedSegment(999u), nullptr);
}

TEST_F(PrefetchManagerTest, InsertCachedSegmentIgnoresNullOrEmptyBuffer) {
  PrefetchManager manager(&timeline_);
  manager.InsertCachedSegment(0, nullptr, {});
  EXPECT_FALSE(manager.HasCachedSegment(0));

  manager.InsertCachedSegment(
      0, media::DecoderBuffer::CopyFrom(std::vector<uint8_t>()), {});
  EXPECT_FALSE(manager.HasCachedSegment(0));
}

TEST_F(PrefetchManagerTest, InsertCachedSegmentIgnoresOutOfBoundsIndex) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Single sentence."});
  ASSERT_EQ(manager.GetTimelineChunkCount(), 1u);

  manager.InsertCachedSegment(
      1,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  EXPECT_FALSE(manager.HasCachedSegment(1));
}

TEST_F(PrefetchManagerTest, SchedulePrefetchThrottlesToMaxConcurrentRequests) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager,
                         {u"Sentence zero.", u"Sentence one.", u"Sentence two.",
                          u"Sentence three.", u"Sentence four."});

  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text,
         read_aloud::mojom::Speaker speaker) { out->push_back(chunk_index); },
      &dispatched_indices));

  for (int i = 0; i < 5; ++i) {
    manager.SchedulePrefetch(i);
  }

  EXPECT_THAT(dispatched_indices, testing::ElementsAre(0u, 1u, 2u));
}

TEST_F(PrefetchManagerTest,
       OnSynthesisResponseRemovesInflightAndSchedulesNext) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Sentence zero.", u"Sentence one.",
                                   u"Sentence two.", u"Sentence three."});

  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text,
         read_aloud::mojom::Speaker speaker) { out->push_back(chunk_index); },
      &dispatched_indices));

  for (int i = 0; i < 4; ++i) {
    manager.SchedulePrefetch(i);
  }

  EXPECT_THAT(dispatched_indices, testing::ElementsAre(0u, 1u, 2u));

  uint64_t seq_id = manager.GetCurrentSequenceId();
  manager.OnSynthesisResponse(
      seq_id, 0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  EXPECT_TRUE(manager.HasCachedSegment(0));
  EXPECT_THAT(dispatched_indices, testing::ElementsAre(0u, 1u, 2u, 3u));
}

TEST_F(PrefetchManagerTest,
       OnSynthesisResponseWithNullBufferReleasesInflightAndSchedulesNext) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Sentence zero.", u"Sentence one.",
                                   u"Sentence two.", u"Sentence three."});

  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text,
         read_aloud::mojom::Speaker speaker) { out->push_back(chunk_index); },
      &dispatched_indices));

  for (size_t i = 0; i <= PrefetchManager::kMaxConcurrentRequests; ++i) {
    manager.SchedulePrefetch(static_cast<uint32_t>(i));
  }

  EXPECT_EQ(dispatched_indices.size(), PrefetchManager::kMaxConcurrentRequests);

  uint64_t seq_id = manager.GetCurrentSequenceId();
  // Simulate synthesis error response with nullptr buffer.
  manager.OnSynthesisResponse(seq_id, 0, nullptr, {});

  // Chunk 0 should not be reported as cached audio, but its status must be
  // recorded as kSynthesisError in session_cache_, in-flight slot freed, and
  // chunk 3 dispatched.
  EXPECT_FALSE(manager.HasCachedSegment(0));
  const CachedCompressedSegment* cached0 = manager.GetCachedSegment(0);
  ASSERT_NE(cached0, nullptr);
  EXPECT_EQ(cached0->status, SynthesisResultStatus::kSynthesisError);
  EXPECT_EQ(cached0->opus_buffer, nullptr);

  ASSERT_EQ(dispatched_indices.size(),
            PrefetchManager::kMaxConcurrentRequests + 1);
  EXPECT_EQ(dispatched_indices.back(),
            static_cast<uint32_t>(PrefetchManager::kMaxConcurrentRequests));
}

TEST_F(PrefetchManagerTest, RecordsSynthesisErrorStatusInCache) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Chunk zero."});

  manager.InsertCachedSegment(0, nullptr, {},
                              SynthesisResultStatus::kSynthesisError);
  EXPECT_FALSE(manager.HasCachedSegment(0));
  const CachedCompressedSegment* cached = manager.GetCachedSegment(0);
  ASSERT_NE(cached, nullptr);
  EXPECT_EQ(cached->status, SynthesisResultStatus::kSynthesisError);
  EXPECT_EQ(cached->opus_buffer, nullptr);
}

TEST_F(PrefetchManagerTest, RecordsCorruptDataStatusInCache) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Chunk zero."});

  scoped_refptr<media::DecoderBuffer> corrupt_buffer =
      media::DecoderBuffer::CopyFrom(std::vector<uint8_t>{0xff, 0xff});
  manager.InsertCachedSegment(0, corrupt_buffer, {},
                              SynthesisResultStatus::kCorruptData);
  EXPECT_FALSE(manager.HasCachedSegment(0));
  const CachedCompressedSegment* cached = manager.GetCachedSegment(0);
  ASSERT_NE(cached, nullptr);
  EXPECT_EQ(cached->status, SynthesisResultStatus::kCorruptData);
}

TEST_F(PrefetchManagerTest, StaleOrOutOrderResponseIsDiscarded) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Hello Chromium."});
  uint64_t old_seq_id = manager.GetCurrentSequenceId();
  manager.SchedulePrefetch(0);

  manager.ResetSession();
  EXPECT_EQ(manager.GetCurrentSequenceId(), old_seq_id + 1);

  manager.OnSynthesisResponse(
      old_seq_id, 0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  EXPECT_FALSE(manager.HasCachedSegment(0));
}

TEST_F(PrefetchManagerTest, SchedulePrefetchIgnoresDuplicatePendingRequest) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager,
                         {u"Sentence zero.", u"Sentence one.", u"Sentence two.",
                          u"Sentence three.", u"Sentence four."});

  // Saturate the concurrency slots up to kMaxConcurrentRequests.
  for (size_t i = 0; i < PrefetchManager::kMaxConcurrentRequests; ++i) {
    manager.SchedulePrefetch(static_cast<uint32_t>(i));
  }

  // Attempt to schedule chunk 3 multiple times while slots are full.
  manager.SchedulePrefetch(3);
  manager.SchedulePrefetch(3);
  manager.SchedulePrefetch(3);

  int dispatch_count_3 = 0;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](int* count_3, uint32_t idx, std::u16string_view text,
         read_aloud::mojom::Speaker speaker) {
        if (idx == 3) {
          (*count_3)++;
        }
      },
      &dispatch_count_3));

  uint64_t seq_id = manager.GetCurrentSequenceId();
  // Completing initial chunks sequentially opens concurrency slots.
  for (size_t i = 0; i < PrefetchManager::kMaxConcurrentRequests; ++i) {
    manager.OnSynthesisResponse(
        seq_id, static_cast<uint32_t>(i),
        media::DecoderBuffer::CopyFrom(
            std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
        {});
  }

  // Chunk 3 should be dispatched exactly once, not three times.
  EXPECT_EQ(dispatch_count_3, 1);
}

TEST_F(PrefetchManagerTest,
       SchedulePrefetchWithNullCallbackDoesNotLeakInflightSlots) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Sentence zero."});

  // Schedule prefetch without setting a request synthesis callback.
  manager.SchedulePrefetch(0);

  // Setting callback later should safely dispatch the pending request.
  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text,
         read_aloud::mojom::Speaker speaker) { out->push_back(chunk_index); },
      &dispatched_indices));

  ASSERT_EQ(dispatched_indices.size(), 1u);
  EXPECT_EQ(dispatched_indices[0], 0u);
}

TEST_F(PrefetchManagerTest, UpdatePrefetchModeDelegatesToModeScheduler) {
  PrefetchManager manager(&timeline_);
  EXPECT_EQ(manager.GetPrefetchMode(), PrefetchMode::kSpeed);

  EXPECT_EQ(manager.UpdatePrefetchMode(base::Seconds(15)),
            PrefetchMode::kQuality);
  EXPECT_EQ(manager.GetPrefetchMode(), PrefetchMode::kQuality);

  manager.ResetSession();
  EXPECT_EQ(manager.GetPrefetchMode(), PrefetchMode::kSpeed);
}

TEST_F(PrefetchManagerTest, GetRequiredPrefetchChunksReturnsUncachedAhead) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager,
                         std::vector<std::u16string>(10, u"Sentence."));

  EXPECT_THAT(manager.GetRequiredPrefetchChunks(0, base::Seconds(0)),
              testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(PrefetchManagerTest, GetRequiredPrefetchChunksSkipsCachedChunks) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager,
                         std::vector<std::u16string>(10, u"Sentence."));

  manager.InsertCachedSegment(
      1,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  manager.InsertCachedSegment(
      3,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  // Window [0, 5) contains chunks 0, 1, 2, 3, 4. Chunks 1 and 3 are cached.
  // The uncached chunks within the 5-chunk lookahead window are 0, 2, 4.
  EXPECT_THAT(manager.GetRequiredPrefetchChunks(0, base::Seconds(0)),
              testing::ElementsAre(0, 2, 4));
}

TEST_F(PrefetchManagerTest,
       GetRequiredPrefetchChunksDoesNotScanBeyondMaxLookahead) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager,
                         std::vector<std::u16string>(10, u"Sentence."));

  // Cache all chunks in the lookahead window [0, 5).
  for (int i = 0; i < 5; ++i) {
    manager.InsertCachedSegment(
        i,
        media::DecoderBuffer::CopyFrom(
            std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
        {});
  }

  // Lookahead window [0, 5) is fully cached; should NOT scan chunks 5..9.
  std::vector<uint32_t> required =
      manager.GetRequiredPrefetchChunks(0, base::Seconds(0));
  EXPECT_TRUE(required.empty());
}

TEST_F(PrefetchManagerTest,
       GetRequiredPrefetchChunksReturnsEmptyWhenWindowFull) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager,
                         std::vector<std::u16string>(10, u"Sentence."));

  std::vector<uint32_t> required =
      manager.GetRequiredPrefetchChunks(0, base::Seconds(15));
  EXPECT_TRUE(required.empty());

  // Negative buffered duration should also return empty vector.
  std::vector<uint32_t> required_neg =
      manager.GetRequiredPrefetchChunks(0, base::Seconds(-1));
  EXPECT_TRUE(required_neg.empty());
}

TEST_F(PrefetchManagerTest, GetRequiredPrefetchChunksRespectsTimelineBounds) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager,
                         std::vector<std::u16string>(10, u"Sentence."));

  EXPECT_THAT(manager.GetRequiredPrefetchChunks(8, base::Seconds(0)),
              testing::ElementsAre(8, 9));
}

TEST_F(PrefetchManagerTest,
       CancelInflightRequestsClearsQueuesAndInvalidatesSequenceId) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(
      manager, {u"Sentence one.", u"Sentence two.", u"Sentence three.",
                u"Sentence four.", u"Sentence five.", u"Sentence six."});
  uint64_t seq_id = manager.GetCurrentSequenceId();

  std::vector<uint32_t> dispatched_chunks;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out_chunks, uint32_t chunk_index,
         std::u16string_view text, read_aloud::mojom::Speaker speaker) {
        out_chunks->push_back(chunk_index);
      },
      &dispatched_chunks));

  // Schedule 5 chunks. Maximum concurrent in-flight is 3, so 3 go to in-flight
  // and 2 go to pending queue.
  for (uint32_t i = 0; i < 5; ++i) {
    manager.SchedulePrefetch(i);
  }
  EXPECT_EQ(dispatched_chunks.size(), 3u);
  EXPECT_EQ(manager.GetInflightRequestCount(), 3u);

  // Cancel inflight and pending requests
  manager.CancelInflightRequests();
  EXPECT_EQ(manager.GetInflightRequestCount(), 0u);
  EXPECT_GT(manager.GetCurrentSequenceId(), seq_id);

  // Stale callback with old sequence_id should be ignored
  manager.OnSynthesisResponse(
      seq_id, 0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  EXPECT_FALSE(manager.HasCachedSegment(0));
}

TEST_F(PrefetchManagerTest, PreservesSpeakerPerSegment) {
  PrefetchManager manager(&timeline_);
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->text = u"Host speaking.";
  seg0->speaker = read_aloud::mojom::Speaker::kSpeaker1;
  segments.push_back(std::move(seg0));
  auto seg1 = read_aloud::mojom::TextSegment::New();
  seg1->text = u"Guest answering.";
  seg1->speaker = read_aloud::mojom::Speaker::kSpeaker2;
  segments.push_back(std::move(seg1));
  timeline_.SetTextContent(std::move(segments));

  std::vector<read_aloud::mojom::Speaker> dispatched_speakers;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<read_aloud::mojom::Speaker>* out, uint32_t chunk_index,
         std::u16string_view text,
         read_aloud::mojom::Speaker speaker) { out->push_back(speaker); },
      &dispatched_speakers));

  manager.SchedulePrefetch(0);
  manager.SchedulePrefetch(1);

  EXPECT_THAT(dispatched_speakers,
              testing::ElementsAre(read_aloud::mojom::Speaker::kSpeaker1,
                                   read_aloud::mojom::Speaker::kSpeaker2));
}

TEST_F(PrefetchManagerTest, ResetSessionCancelsInflightRequests) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Old sentence."});
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](uint32_t /*chunk_index*/, std::u16string_view /*text*/,
         read_aloud::mojom::Speaker /*speaker*/) {}));

  manager.SchedulePrefetch(0);
  EXPECT_EQ(manager.GetInflightRequestCount(), 1u);

  manager.ResetSession();
  EXPECT_EQ(manager.GetInflightRequestCount(), 0u);
}

TEST_F(PrefetchManagerTest, EmptyTimelineDisablesPrefetch) {
  PrefetchManager manager(&timeline_);
  uint32_t callback_count = 0;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](uint32_t* count, uint32_t /*chunk_index*/,
         std::u16string_view /*text*/,
         read_aloud::mojom::Speaker /*speaker*/) { (*count)++; },
      &callback_count));
  SetTimelineFromStrings(manager, {u"Initial sentence."});
  EXPECT_EQ(manager.GetTimelineChunkCount(), 1u);

  manager.ResetSession();
  timeline_.Clear();
  timeline_.SetTextContent({});
  EXPECT_EQ(manager.GetTimelineChunkCount(), 0u);

  manager.SchedulePrefetch(0);
  EXPECT_EQ(manager.GetInflightRequestCount(), 0u);
  EXPECT_EQ(callback_count, 0u);
}

TEST_F(PrefetchManagerTest, EmptyTimelineRequiresNoPrefetchChunks) {
  PrefetchManager manager(&timeline_);
  timeline_.SetTextContent({});
  EXPECT_EQ(manager.GetTimelineChunkCount(), 0u);
  EXPECT_THAT(manager.GetRequiredPrefetchChunks(0, base::Seconds(0)),
              testing::IsEmpty());
}

TEST_F(PrefetchManagerTest, SkipsPendingChunkCachedBeforeDequeue) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Chunk 0.", u"Chunk 1.", u"Chunk 2.",
                                   u"Chunk 3.", u"Chunk 4."});

  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* indices, uint32_t chunk_index,
         std::u16string_view /*text*/, read_aloud::mojom::Speaker /*speaker*/) {
        indices->push_back(chunk_index);
      },
      &dispatched_indices));

  // Schedule 5 chunks: 0, 1, 2 go in-flight; 3 and 4 sit in pending_requests_.
  for (uint32_t i = 0; i < 5; ++i) {
    manager.SchedulePrefetch(i);
  }
  EXPECT_THAT(dispatched_indices, testing::ElementsAre(0u, 1u, 2u));
  EXPECT_EQ(manager.GetInflightRequestCount(), 3u);

  // Cache chunk 3 while it is still waiting in pending_requests_.
  manager.InsertCachedSegment(
      3,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  // Completing chunk 0 frees an in-flight slot; chunk 3 is skipped because it
  // is now cached, and chunk 4 is dispatched instead.
  manager.OnSynthesisResponse(
      manager.GetCurrentSequenceId(), 0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  EXPECT_THAT(dispatched_indices, testing::ElementsAre(0u, 1u, 2u, 4u));
  EXPECT_EQ(manager.GetInflightRequestCount(), 3u);
}

TEST_F(PrefetchManagerTest, SkipsOutOfBoundsPendingChunk) {
  PrefetchManager manager(&timeline_);
  SetTimelineFromStrings(manager, {u"Chunk 0.", u"Chunk 1.", u"Chunk 2.",
                                   u"Chunk 3.", u"Chunk 4."});

  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* indices, uint32_t chunk_index,
         std::u16string_view /*text*/, read_aloud::mojom::Speaker /*speaker*/) {
        indices->push_back(chunk_index);
      },
      &dispatched_indices));

  // Schedule 5 chunks: 0, 1, 2 go in-flight; 3 and 4 sit in pending_requests_.
  for (uint32_t i = 0; i < 5; ++i) {
    manager.SchedulePrefetch(i);
  }
  EXPECT_THAT(dispatched_indices, testing::ElementsAre(0u, 1u, 2u));

  // Shrink timeline to 4 chunks (indices 0..3) without resetting manager, so
  // pending chunk 4 is now out of bounds when dequeued.
  timeline_.Clear();
  std::vector<read_aloud::mojom::TextSegmentPtr> shorter_segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->text = u"Chunk 0. Chunk 1. Chunk 2. Chunk 3.";
  shorter_segments.push_back(std::move(seg));
  timeline_.SetTextContent(std::move(shorter_segments));
  ASSERT_EQ(manager.GetTimelineChunkCount(), 4u);

  // Completing chunks 0 and 1 dequeues chunk 3 (valid) and skips chunk 4
  // (out of bounds).
  const uint64_t seq = manager.GetCurrentSequenceId();
  manager.OnSynthesisResponse(
      seq, 0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  manager.OnSynthesisResponse(
      seq, 1,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  EXPECT_THAT(dispatched_indices, testing::ElementsAre(0u, 1u, 2u, 3u));
  EXPECT_EQ(manager.GetInflightRequestCount(), 2u);
}

}  // namespace readaloud
