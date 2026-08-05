// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

using PrefetchManagerTest = testing::Test;

TEST_F(PrefetchManagerTest, DefaultConstructor) {
  PrefetchManager manager;
  EXPECT_FALSE(manager.HasCachedSegment(0));
  EXPECT_EQ(nullptr, manager.GetCachedSegment(0));
  EXPECT_EQ(0u, manager.GetTimelineChunkCount());
  EXPECT_TRUE(manager.GetTimelineChunks().empty());
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

TEST_F(PrefetchManagerTest, GetCachedSegmentWithNegativeIndexReturnsNull) {
  PrefetchManager manager;
  manager.InsertCachedSegment(
      -1,
      media::DecoderBuffer::CopyFrom(
          std::vector<uint8_t>({0x4F, 0x67, 0x67, 0x53})),
      {});

  EXPECT_FALSE(manager.HasCachedSegment(-1));
  EXPECT_EQ(nullptr, manager.GetCachedSegment(-1));
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

}  // namespace readaloud
