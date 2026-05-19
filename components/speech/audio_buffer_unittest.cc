// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/speech/audio_buffer.h"

#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

TEST(AudioChunkTest, Constructors) {
  constexpr int kBytesPerSample = 2;
  constexpr size_t kLength = 10;

  // Empty constructor
  auto chunk1 = base::MakeRefCounted<AudioChunk>(kBytesPerSample);
  EXPECT_TRUE(chunk1->IsEmpty());
  EXPECT_EQ(chunk1->bytes_per_sample(), kBytesPerSample);
  EXPECT_EQ(chunk1->data().size(), 0u);

  // Length constructor
  auto chunk2 = base::MakeRefCounted<AudioChunk>(kLength, kBytesPerSample);
  EXPECT_FALSE(chunk2->IsEmpty());
  EXPECT_EQ(chunk2->data().size(), kLength);
  for (uint8_t byte : chunk2->data()) {
    EXPECT_EQ(byte, 0);
  }

  // Data constructor
  constexpr auto kData =
      std::to_array<uint8_t>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  auto chunk3 = base::MakeRefCounted<AudioChunk>(kData.data(), kData.size(),
                                                 kBytesPerSample);
  EXPECT_EQ(chunk3->data().size(), kData.size());
  EXPECT_TRUE(base::span(chunk3->data()) == base::span(kData));

  // Span constructor
  base::span<const uint8_t> data_span(kData);
  auto chunk4 = base::MakeRefCounted<AudioChunk>(data_span, kBytesPerSample);
  EXPECT_EQ(chunk4->data().size(), data_span.size());
  EXPECT_TRUE(base::span(chunk4->data()) == data_span);
}

TEST(AudioChunkTest, EmptyLength) {
  constexpr int kBytesPerSample = 1;
  constexpr size_t kLength = 0;

  // Length constructor
  auto chunk = base::MakeRefCounted<AudioChunk>(kLength, kBytesPerSample);
  EXPECT_TRUE(chunk->IsEmpty());

  // Data constructor
  constexpr auto kData = std::array<uint8_t, 0>();
  auto chunk2 = base::MakeRefCounted<AudioChunk>(kData.data(), kData.size(),
                                                 kBytesPerSample);
  EXPECT_TRUE(chunk2->IsEmpty());

  // Span constructor
  base::span<const uint8_t> data_span(kData);
  auto chunk3 = base::MakeRefCounted<AudioChunk>(data_span, kBytesPerSample);
  EXPECT_TRUE(chunk3->IsEmpty());
}

TEST(AudioChunkTest, AsStringView) {
  constexpr int kBytesPerSample = 1;
  constexpr auto kData = std::to_array<uint8_t>({'H', 'e', 'l', 'l', 'o'});
  auto chunk = base::MakeRefCounted<AudioChunk>(kData.data(), kData.size(),
                                                kBytesPerSample);
  EXPECT_EQ(chunk->AsStringView(), "Hello");
}

TEST(AudioChunkTest, WritableData) {
  constexpr int kBytesPerSample = 1;
  constexpr size_t kLength = 5;
  auto chunk = base::MakeRefCounted<AudioChunk>(kLength, kBytesPerSample);

  base::span<uint8_t> writable = chunk->writable_data();
  ASSERT_EQ(writable.size(), kLength);
  for (size_t i = 0; i < kLength; ++i) {
    writable[i] = static_cast<uint8_t>(i + 1);
  }

  for (size_t i = 0; i < kLength; ++i) {
    EXPECT_EQ(chunk->data()[i], i + 1);
  }
}

TEST(AudioChunkTest, NumSamples) {
  constexpr int kBytesPerSample = 2;
  constexpr size_t kLength = 10;
  auto chunk = base::MakeRefCounted<AudioChunk>(kLength, kBytesPerSample);
  EXPECT_EQ(chunk->NumSamples(), kLength / kBytesPerSample);
}

TEST(AudioChunkTest, GetSample16) {
  constexpr int kBytesPerSample = 2;
  constexpr auto kSamples = std::to_array<int16_t>({0x0102, 0x0304, 0x0506});
  base::span<const uint8_t> data = base::as_byte_span(kSamples);

  auto chunk = base::MakeRefCounted<AudioChunk>(data.data(), data.size(),
                                                kBytesPerSample);
  ASSERT_EQ(chunk->NumSamples(), 3u);
  EXPECT_EQ(chunk->GetSample16(0), kSamples[0]);
  EXPECT_EQ(chunk->GetSample16(1), kSamples[1]);
  EXPECT_EQ(chunk->GetSample16(2), kSamples[2]);

  auto span = chunk->SamplesData16AsSpan();
  ASSERT_EQ(span.size(), 3u);
  EXPECT_EQ(span[0], kSamples[0]);
  EXPECT_EQ(span[1], kSamples[1]);
  EXPECT_EQ(span[2], kSamples[2]);
}

TEST(AudioBufferTest, Basic) {
  constexpr int kBytesPerSample = 2;
  AudioBuffer buffer(kBytesPerSample);
  EXPECT_TRUE(buffer.IsEmpty());

  constexpr auto kData1 = std::to_array<uint8_t>({1, 2, 3, 4});
  buffer.Enqueue(kData1.data(), kData1.size());
  EXPECT_FALSE(buffer.IsEmpty());

  constexpr auto kData2 = std::to_array<uint8_t>({5, 6, 7, 8, 9, 10});
  buffer.Enqueue(kData2.data(), kData2.size());

  // DequeueSingleChunk
  auto chunk1 = buffer.DequeueSingleChunk();
  EXPECT_EQ(chunk1->data().size(), kData1.size());
  EXPECT_TRUE(base::span(chunk1->data()) == base::span(kData1));
  EXPECT_FALSE(buffer.IsEmpty());

  auto chunk2 = buffer.DequeueSingleChunk();
  EXPECT_EQ(chunk2->data().size(), kData2.size());
  EXPECT_TRUE(base::span(chunk2->data()) == base::span(kData2));
  EXPECT_TRUE(buffer.IsEmpty());
}

TEST(AudioBufferTest, DequeueAll) {
  constexpr int kBytesPerSample = 2;
  AudioBuffer buffer(kBytesPerSample);

  constexpr auto kData1 = std::to_array<uint8_t>({1, 2, 3, 4});
  constexpr auto kData2 = std::to_array<uint8_t>({5, 6, 7, 8, 9, 10});
  buffer.Enqueue(kData1.data(), kData1.size());
  buffer.Enqueue(kData2.data(), kData2.size());

  auto merged = buffer.DequeueAll();
  EXPECT_TRUE(buffer.IsEmpty());
  EXPECT_EQ(merged->data().size(), kData1.size() + kData2.size());

  constexpr auto kExpected =
      std::to_array<uint8_t>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  EXPECT_TRUE(base::span(merged->data()) == base::span(kExpected));
}

TEST(AudioBufferTest, Clear) {
  constexpr int kBytesPerSample = 2;
  AudioBuffer buffer(kBytesPerSample);

  constexpr auto kData = std::to_array<uint8_t>({1, 2, 3, 4});
  buffer.Enqueue(kData.data(), kData.size());
  EXPECT_FALSE(buffer.IsEmpty());

  buffer.Clear();
  EXPECT_TRUE(buffer.IsEmpty());
}

TEST(AudioBufferTest, DequeueEmpty) {
  constexpr int kBytesPerSample = 2;
  AudioBuffer buffer(kBytesPerSample);
  ASSERT_TRUE(buffer.IsEmpty());

  // DequeueAll should return an empty chunk.
  auto merged = buffer.DequeueAll();
  EXPECT_TRUE(merged->IsEmpty());
  EXPECT_EQ(merged->data().size(), 0u);
}

}  // namespace speech
