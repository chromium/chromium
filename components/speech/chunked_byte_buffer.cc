// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/speech/chunked_byte_buffer.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/lazy_instance.h"
#include "base/numerics/byte_conversions.h"

namespace {

static const size_t kHeaderLength = sizeof(uint32_t);

static_assert(sizeof(size_t) >= kHeaderLength,
              "chunked byte buffer not supported on this architecture");

}  // namespace

namespace speech {

ChunkedByteBuffer::ChunkedByteBuffer()
    : partial_chunk_(new Chunk()), total_bytes_stored_(0) {}

ChunkedByteBuffer::~ChunkedByteBuffer() {
  Clear();
}

void ChunkedByteBuffer::Append(const uint8_t* start, size_t length) {
  size_t remaining_bytes = length;
  const uint8_t* next_data = start;

  while (remaining_bytes > 0) {
    DCHECK(partial_chunk_ != nullptr);
    size_t insert_length = 0;
    bool header_completed = false;
    bool content_completed = false;
    std::vector<uint8_t>* insert_target;

    if (partial_chunk_->header.size() < kHeaderLength) {
      const size_t bytes_to_complete_header =
          kHeaderLength - partial_chunk_->header.size();
      insert_length = std::min(bytes_to_complete_header, remaining_bytes);
      insert_target = &partial_chunk_->header;
      header_completed = (remaining_bytes >= bytes_to_complete_header);
    } else {
      DCHECK_LT(partial_chunk_->content->size(),
                partial_chunk_->ExpectedContentLength());
      const size_t bytes_to_complete_chunk =
          partial_chunk_->ExpectedContentLength() -
          partial_chunk_->content->size();
      insert_length = std::min(bytes_to_complete_chunk, remaining_bytes);
      insert_target = partial_chunk_->content.get();
      content_completed = (remaining_bytes >= bytes_to_complete_chunk);
    }

    DCHECK_GT(insert_length, 0U);
    DCHECK_LE(insert_length, remaining_bytes);
    DCHECK_LE(next_data + insert_length, start + length);
    insert_target->insert(insert_target->end(), next_data,
                          next_data + insert_length);
    next_data += insert_length;
    remaining_bytes -= insert_length;

    if (header_completed) {
      DCHECK_EQ(partial_chunk_->header.size(), kHeaderLength);
      if (partial_chunk_->ExpectedContentLength() == 0) {
        // Handle zero-byte chunks.
        chunks_.push_back(std::move(partial_chunk_));
        partial_chunk_ = std::make_unique<Chunk>();
      } else {
        partial_chunk_->content->reserve(
            partial_chunk_->ExpectedContentLength());
      }
    } else if (content_completed) {
      DCHECK_EQ(partial_chunk_->content->size(),
                partial_chunk_->ExpectedContentLength());
      chunks_.push_back(std::move(partial_chunk_));
      partial_chunk_ = std::make_unique<Chunk>();
    }
  }
  DCHECK_EQ(next_data, start + length);
  total_bytes_stored_ += length;
}

void ChunkedByteBuffer::Append(std::string_view string) {
  Append(reinterpret_cast<const uint8_t*>(string.data()), string.size());
}

bool ChunkedByteBuffer::HasChunks() const {
  return !chunks_.empty();
}

std::unique_ptr<std::vector<uint8_t>> ChunkedByteBuffer::PopChunk() {
  if (chunks_.empty())
    return nullptr;
  std::unique_ptr<Chunk> chunk = std::move(*chunks_.begin());
  chunks_.erase(chunks_.begin());
  DCHECK_EQ(chunk->header.size(), kHeaderLength);
  DCHECK_EQ(chunk->content->size(), chunk->ExpectedContentLength());
  total_bytes_stored_ -= chunk->content->size();
  total_bytes_stored_ -= kHeaderLength;
  return std::move(chunk->content);
}

void ChunkedByteBuffer::Clear() {
  chunks_.clear();
  partial_chunk_ = std::make_unique<Chunk>();
  total_bytes_stored_ = 0;
}

ChunkedByteBuffer::Chunk::Chunk() : content(new std::vector<uint8_t>()) {}

ChunkedByteBuffer::Chunk::~Chunk() {}

size_t ChunkedByteBuffer::Chunk::ExpectedContentLength() const {
  DCHECK_EQ(header.size(), kHeaderLength);
  return base::U32FromBigEndian(base::span(header).first<4>());
}

}  // namespace speech
