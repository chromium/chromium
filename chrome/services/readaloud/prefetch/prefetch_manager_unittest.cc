// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/language_tag.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class PrefetchManagerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PrefetchManagerTest, DefaultConstructor) {
  PrefetchManager manager;
  EXPECT_FALSE(manager.HasCachedSegment(0));
  EXPECT_EQ(nullptr, manager.GetCachedSegment(0));
  EXPECT_EQ(0u, manager.GetTimelineChunkCount());
  EXPECT_TRUE(manager.GetTimelineChunks().empty());
  EXPECT_EQ(0u, manager.GetCurrentSequenceId());
  EXPECT_EQ(0u, manager.GetInflightRequestCount());
}

TEST_F(PrefetchManagerTest, InsertAndRetrieveCachedSegment) {
  PrefetchManager manager;
  std::vector<DecodedAudioSegment::WordTiming> timings = {
      {"Hello", base::Milliseconds(0), base::Milliseconds(200)},
      {"World", base::Milliseconds(200), base::Milliseconds(500)}};

  manager.InsertCachedSegment(
      0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      timings);

  EXPECT_TRUE(manager.HasCachedSegment(0));
  EXPECT_FALSE(manager.HasCachedSegment(1));

  const CachedCompressedSegment* cached = manager.GetCachedSegment(0);
  ASSERT_NE(nullptr, cached);
  ASSERT_NE(nullptr, cached->opus_buffer);
  EXPECT_EQ(4u, cached->opus_buffer->size());
  EXPECT_EQ(0x4F, *cached->opus_buffer->begin());
  ASSERT_EQ(2u, cached->timings.size());
  EXPECT_EQ("Hello", cached->timings[0].text);
  EXPECT_EQ(base::Milliseconds(0), cached->timings[0].start_time);
  EXPECT_EQ(base::Milliseconds(200), cached->timings[0].end_time);
  EXPECT_EQ("World", cached->timings[1].text);
}

TEST_F(PrefetchManagerTest, SetTextContentPopulatesTimelineAndClearsCache) {
  PrefetchManager manager;
  manager.InsertCachedSegment(
      0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  EXPECT_TRUE(manager.HasCachedSegment(0));

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  {
    read_aloud::mojom::TextSegmentPtr seg =
        read_aloud::mojom::TextSegment::New();
    seg->segment_index = 0;
    seg->text = u"First sentence. Second sentence!";
    segments.push_back(std::move(seg));
  }
  {
    read_aloud::mojom::TextSegmentPtr seg =
        read_aloud::mojom::TextSegment::New();
    seg->segment_index = 1;
    seg->text = u"Third sentence? Fourth sentence.";
    segments.push_back(std::move(seg));
  }

  manager.SetTextContent(segments, base::i18n::GetKnownLanguageTag("en-US"));

  // Verify document-bound reset cleared the old session cache.
  EXPECT_FALSE(manager.HasCachedSegment(0));
  EXPECT_EQ(nullptr, manager.GetCachedSegment(0));

  // Verify canonical sentence timeline was generated in kSpeed mode.
  EXPECT_EQ(4u, manager.GetTimelineChunkCount());
  EXPECT_FALSE(manager.GetTimelineChunks().empty());
}

TEST_F(PrefetchManagerTest, SetTextContentFiresOnTextChunkedCallback) {
  PrefetchManager manager;

  std::vector<std::u16string> received_chunks;
  int callback_count = 0;
  manager.SetOnTextChunkedCallback(base::BindRepeating(
      [](std::vector<std::u16string>* out_chunks, int* count,
         const std::vector<std::u16string>& chunks) {
        *out_chunks = chunks;
        (*count)++;
      },
      base::Unretained(&received_chunks), base::Unretained(&callback_count)));

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"First sentence. Second sentence!";
  segments.push_back(std::move(seg));

  manager.SetTextContent(segments);

  EXPECT_EQ(callback_count, 1);
  EXPECT_THAT(received_chunks,
              testing::ElementsAre(u"First sentence.", u"Second sentence!"));
}

TEST_F(PrefetchManagerTest,
       SetTextContentEmptySegmentsFiresOnTextChunkedCallback) {
  PrefetchManager manager;

  std::vector<std::u16string> received_chunks;
  int callback_count = 0;
  manager.SetOnTextChunkedCallback(base::BindRepeating(
      [](std::vector<std::u16string>* out_chunks, int* count,
         const std::vector<std::u16string>& chunks) {
        *out_chunks = chunks;
        (*count)++;
      },
      base::Unretained(&received_chunks), base::Unretained(&callback_count)));

  manager.SetTextContent({});
  EXPECT_EQ(callback_count, 1);
  EXPECT_TRUE(received_chunks.empty());
}

TEST_F(PrefetchManagerTest, ResetSessionClearsCacheAndTimeline) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Hello Chromium.";
  segments.push_back(std::move(seg));

  manager.SetTextContent(segments);
  manager.InsertCachedSegment(
      0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  EXPECT_EQ(1u, manager.GetTimelineChunkCount());
  EXPECT_TRUE(manager.HasCachedSegment(0));

  manager.ResetSession();

  EXPECT_EQ(0u, manager.GetTimelineChunkCount());
  EXPECT_FALSE(manager.HasCachedSegment(0));
}

TEST_F(PrefetchManagerTest, ClearCachePurgesAudioWithoutClearingTimeline) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Hello Chromium.";
  segments.push_back(std::move(seg));

  manager.SetTextContent(segments);
  manager.InsertCachedSegment(
      0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  EXPECT_EQ(1u, manager.GetTimelineChunkCount());
  EXPECT_TRUE(manager.HasCachedSegment(0));

  manager.ClearCache();

  EXPECT_EQ(1u, manager.GetTimelineChunkCount());
  EXPECT_FALSE(manager.HasCachedSegment(0));
}

TEST_F(PrefetchManagerTest, GetCachedSegmentWithUncachedIndexReturnsNull) {
  PrefetchManager manager;
  EXPECT_FALSE(manager.HasCachedSegment(999u));
  EXPECT_EQ(nullptr, manager.GetCachedSegment(999u));
}

TEST_F(PrefetchManagerTest, InsertCachedSegmentIgnoresNullOrEmptyBuffer) {
  PrefetchManager manager;
  manager.InsertCachedSegment(0, nullptr, {});
  EXPECT_FALSE(manager.HasCachedSegment(0));

  manager.InsertCachedSegment(
      0, media::DecoderBuffer::CopyFrom(std::vector<uint8_t>()), {});
  EXPECT_FALSE(manager.HasCachedSegment(0));
}

TEST_F(PrefetchManagerTest, SetTextContentSkipsEmptyAndNullSegments) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(nullptr);

  read_aloud::mojom::TextSegmentPtr empty_segment =
      read_aloud::mojom::TextSegment::New();
  empty_segment->segment_index = 0;
  empty_segment->text = u"";
  segments.push_back(std::move(empty_segment));

  read_aloud::mojom::TextSegmentPtr valid_segment =
      read_aloud::mojom::TextSegment::New();
  valid_segment->segment_index = 1;
  valid_segment->text = u"Valid sentence.";
  segments.push_back(std::move(valid_segment));

  manager.SetTextContent(segments);
  EXPECT_EQ(1u, manager.GetTimelineChunkCount());
}

TEST_F(PrefetchManagerTest, InsertCachedSegmentIgnoresOutOfBoundsIndex) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr segment =
      read_aloud::mojom::TextSegment::New();
  segment->segment_index = 0;
  segment->text = u"Single sentence.";
  segments.push_back(std::move(segment));

  manager.SetTextContent(segments);
  ASSERT_EQ(1u, manager.GetTimelineChunkCount());

  manager.InsertCachedSegment(
      1,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  EXPECT_FALSE(manager.HasCachedSegment(1));
}
TEST_F(PrefetchManagerTest, SchedulePrefetchThrottlesToMaxConcurrentRequests) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  const std::vector<std::u16string> kTexts = {
      u"Sentence zero.", u"Sentence one.", u"Sentence two.", u"Sentence three.",
      u"Sentence four."};
  for (size_t i = 0; i < kTexts.size(); ++i) {
    read_aloud::mojom::TextSegmentPtr seg =
        read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = kTexts[i];
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text) { out->push_back(chunk_index); },
      &dispatched_indices));

  for (int i = 0; i < 5; ++i) {
    manager.SchedulePrefetch(i);
  }

  ASSERT_EQ(3u, dispatched_indices.size());
  EXPECT_EQ(0u, dispatched_indices[0]);
  EXPECT_EQ(1u, dispatched_indices[1]);
  EXPECT_EQ(2u, dispatched_indices[2]);
}

TEST_F(PrefetchManagerTest,
       OnSynthesisResponseRemovesInflightAndSchedulesNext) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  const std::vector<std::u16string> kTexts = {
      u"Sentence zero.", u"Sentence one.", u"Sentence two.",
      u"Sentence three."};
  for (size_t i = 0; i < kTexts.size(); ++i) {
    read_aloud::mojom::TextSegmentPtr seg =
        read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = kTexts[i];
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text) { out->push_back(chunk_index); },
      &dispatched_indices));

  for (int i = 0; i < 4; ++i) {
    manager.SchedulePrefetch(i);
  }

  EXPECT_EQ(3u, dispatched_indices.size());

  uint64_t seq_id = manager.GetCurrentSequenceId();
  manager.OnSynthesisResponse(
      seq_id, 0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  EXPECT_TRUE(manager.HasCachedSegment(0));
  ASSERT_EQ(4u, dispatched_indices.size());
  EXPECT_EQ(3u, dispatched_indices[3]);
}

TEST_F(PrefetchManagerTest,
       OnSynthesisResponseWithNullBufferReleasesInflightAndSchedulesNext) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  const std::vector<std::u16string> kTexts = {
      u"Sentence zero.", u"Sentence one.", u"Sentence two.",
      u"Sentence three."};
  for (size_t i = 0; i < kTexts.size(); ++i) {
    read_aloud::mojom::TextSegmentPtr seg =
        read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = kTexts[i];
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text) { out->push_back(chunk_index); },
      &dispatched_indices));

  for (size_t i = 0; i <= PrefetchManager::kMaxConcurrentRequests; ++i) {
    manager.SchedulePrefetch(static_cast<uint32_t>(i));
  }

  EXPECT_EQ(PrefetchManager::kMaxConcurrentRequests, dispatched_indices.size());

  uint64_t seq_id = manager.GetCurrentSequenceId();
  // Simulate synthesis error response with nullptr buffer.
  manager.OnSynthesisResponse(seq_id, 0, nullptr, {});

  // Chunk 0 should not be cached, but in-flight slot must be freed and chunk 3
  // dispatched.
  EXPECT_FALSE(manager.HasCachedSegment(0));
  ASSERT_EQ(PrefetchManager::kMaxConcurrentRequests + 1,
            dispatched_indices.size());
  EXPECT_EQ(static_cast<uint32_t>(PrefetchManager::kMaxConcurrentRequests),
            dispatched_indices.back());
}

TEST_F(PrefetchManagerTest, StaleOrOutOrderResponseIsDiscarded) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Hello Chromium.";
  segments.push_back(std::move(seg));

  manager.SetTextContent(segments);
  uint64_t old_seq_id = manager.GetCurrentSequenceId();
  manager.SchedulePrefetch(0);

  manager.ResetSession();
  EXPECT_EQ(old_seq_id + 1, manager.GetCurrentSequenceId());

  manager.OnSynthesisResponse(
      old_seq_id, 0,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});
  EXPECT_FALSE(manager.HasCachedSegment(0));
}

TEST_F(PrefetchManagerTest, SchedulePrefetchIgnoresDuplicatePendingRequest) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  const std::vector<std::u16string> kTexts = {
      u"Sentence zero.", u"Sentence one.", u"Sentence two.", u"Sentence three.",
      u"Sentence four."};
  for (size_t i = 0; i < kTexts.size(); ++i) {
    read_aloud::mojom::TextSegmentPtr seg =
        read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = kTexts[i];
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

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
      [](int* count_3, uint32_t idx, std::u16string_view text) {
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
  EXPECT_EQ(1, dispatch_count_3);
}

TEST_F(PrefetchManagerTest,
       SchedulePrefetchWithNullCallbackDoesNotLeakInflightSlots) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Sentence zero.";
  segments.push_back(std::move(seg));
  manager.SetTextContent(segments);

  // Schedule prefetch without setting a request synthesis callback.
  manager.SchedulePrefetch(0);

  // Setting callback later should safely dispatch the pending request.
  std::vector<uint32_t> dispatched_indices;
  manager.SetRequestSynthesisCallback(base::BindRepeating(
      [](std::vector<uint32_t>* out, uint32_t chunk_index,
         std::u16string_view text) { out->push_back(chunk_index); },
      &dispatched_indices));

  ASSERT_EQ(1u, dispatched_indices.size());
  EXPECT_EQ(0u, dispatched_indices[0]);
}

TEST_F(PrefetchManagerTest, UpdatePrefetchModeDelegatesToModeScheduler) {
  PrefetchManager manager;
  EXPECT_EQ(ChunkingMode::kSpeed, manager.GetChunkingMode());

  EXPECT_EQ(ChunkingMode::kQuality,
            manager.UpdatePrefetchMode(base::Seconds(15)));
  EXPECT_EQ(ChunkingMode::kQuality, manager.GetChunkingMode());

  manager.ResetSession();
  EXPECT_EQ(ChunkingMode::kSpeed, manager.GetChunkingMode());
}

TEST_F(PrefetchManagerTest, GetRequiredPrefetchChunksReturnsUncachedAhead) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  for (int i = 0; i < 10; ++i) {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = u"Sentence.";
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

  EXPECT_THAT(manager.GetRequiredPrefetchChunks(0, base::Seconds(0)),
              testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(PrefetchManagerTest, GetRequiredPrefetchChunksSkipsCachedChunks) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  for (int i = 0; i < 10; ++i) {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = u"Sentence.";
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

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
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  for (int i = 0; i < 10; ++i) {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = u"Sentence.";
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

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
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  for (int i = 0; i < 10; ++i) {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = u"Sentence.";
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

  std::vector<uint32_t> required =
      manager.GetRequiredPrefetchChunks(0, base::Seconds(15));
  EXPECT_TRUE(required.empty());

  // Negative buffered duration should also return empty vector.
  std::vector<uint32_t> required_neg =
      manager.GetRequiredPrefetchChunks(0, base::Seconds(-1));
  EXPECT_TRUE(required_neg.empty());
}

TEST_F(PrefetchManagerTest, GetRequiredPrefetchChunksRespectsTimelineBounds) {
  PrefetchManager manager;
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  for (int i = 0; i < 10; ++i) {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = u"Sentence.";
    segments.push_back(std::move(seg));
  }
  manager.SetTextContent(segments);

  EXPECT_THAT(manager.GetRequiredPrefetchChunks(8, base::Seconds(0)),
              testing::ElementsAre(8, 9));
}

}  // namespace readaloud
