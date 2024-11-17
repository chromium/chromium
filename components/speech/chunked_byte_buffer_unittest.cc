// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/speech/chunked_byte_buffer.h"

#include <stdint.h>

#include <cstring>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

typedef std::vector<uint8_t> ByteVector;

TEST(ChunkedByteBufferTest, BasicTest) {
  ChunkedByteBuffer buffer;

  constexpr auto kChunks = std::to_array<uint8_t>({
      0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04,  // Chunk 1: 4 bytes
      0x00, 0x00, 0x00, 0x02, 0x05, 0x06,              // Chunk 2: 2 bytes
      0x00, 0x00, 0x00, 0x01, 0x07                     // Chunk 3: 1 bytes
  });
  base::SpanReader reader{base::span(kChunks)};

  EXPECT_EQ(0U, buffer.GetTotalLength());
  EXPECT_FALSE(buffer.HasChunks());

  // Append partially chunk 1.
  buffer.Append(base::as_string_view(*reader.Read(2u)));
  EXPECT_EQ(2U, buffer.GetTotalLength());
  EXPECT_FALSE(buffer.HasChunks());

  // Complete chunk 1.
  buffer.Append(base::as_string_view(*reader.Read(6u)));
  EXPECT_EQ(8U, buffer.GetTotalLength());
  EXPECT_TRUE(buffer.HasChunks());

  // Append fully chunk 2.
  buffer.Append(base::as_string_view(*reader.Read(6u)));
  EXPECT_EQ(14U, buffer.GetTotalLength());
  EXPECT_TRUE(buffer.HasChunks());

  // Remove and check chunk 1.
  std::unique_ptr<ByteVector> chunk;
  chunk = buffer.PopChunk();
  EXPECT_TRUE(chunk != nullptr);
  EXPECT_EQ(4U, chunk->size());
  EXPECT_EQ(*chunk, base::span(kChunks).subspan(4, 4));
  EXPECT_EQ(6U, buffer.GetTotalLength());
  EXPECT_TRUE(buffer.HasChunks());

  // Read and check chunk 2.
  chunk = buffer.PopChunk();
  EXPECT_TRUE(chunk != nullptr);
  EXPECT_EQ(2U, chunk->size());
  EXPECT_EQ(*chunk, base::span(kChunks).subspan(12, 2));
  EXPECT_EQ(0U, buffer.GetTotalLength());
  EXPECT_FALSE(buffer.HasChunks());

  // Append fully chunk 3.
  buffer.Append(base::as_string_view(*reader.Read(5u)));
  EXPECT_EQ(5U, buffer.GetTotalLength());

  // Remove and check chunk 3.
  chunk = buffer.PopChunk();
  EXPECT_TRUE(chunk != nullptr);
  EXPECT_EQ(1U, chunk->size());
  EXPECT_EQ((*chunk)[0], kChunks[18]);
  EXPECT_EQ(0U, buffer.GetTotalLength());
  EXPECT_FALSE(buffer.HasChunks());
}

}  // namespace speech
