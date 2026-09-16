// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/chunk_queue.h"

#include <stdint.h>

#include <cstddef>
#include <optional>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "mojo/public/c/system/types.h"

namespace updater {

ChunkQueue::ChunkQueue() : pending_data_([[NSMutableArray alloc] init]) {}

ChunkQueue::~ChunkQueue() = default;

bool ChunkQueue::empty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_data_.count == 0;
}

void ChunkQueue::Push(NSData* data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [pending_data_ addObject:[data copy]];
}

MojoResult ChunkQueue::Consume(
    base::RepeatingCallback<MojoResult(base::span<const uint8_t> slice,
                                       size_t& bytes_written)> write_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (pending_data_.count > 0) {
    NSData* topmost = pending_data_[0];
    __block std::optional<MojoResult> block_result;
    __block size_t consumed_in_chunk = 0;

    [topmost enumerateByteRangesUsingBlock:^(const void* bytes,
                                             NSRange byteRange, BOOL* stop) {
      size_t range_start = byteRange.location;
      size_t range_end = byteRange.location + byteRange.length;

      // Skip ranges that are entirely before our current read offset.
      if (range_end <= topmost_chunk_offset_) {
        return;
      }
      // Calculate offset within the current range.
      size_t offset_in_range = range_start < topmost_chunk_offset_
                                   ? topmost_chunk_offset_ - range_start
                                   : 0;

      // SAFETY: `enumerateByteRangesUsingBlock` guarantees that `bytes` points
      // to at least `byteRange.length` bytes of readable memory for the
      // lifetime of the block.
      auto slice = UNSAFE_BUFFERS(base::span<const uint8_t>(
                                      static_cast<const uint8_t*>(bytes),
                                      byteRange.length))
                       .subspan(offset_in_range);
      size_t written = 0;
      MojoResult result = write_callback.Run(slice, written);

      if (result == MOJO_RESULT_OK) {
        consumed_in_chunk += written;
        if (written < slice.size()) {
          block_result = MOJO_RESULT_SHOULD_WAIT;
          *stop = YES;
        }
      } else {
        block_result = result;
        consumed_in_chunk += written;
        *stop = YES;
      }
    }];

    topmost_chunk_offset_ += consumed_in_chunk;

    // If the topmost chunk was fully consumed, pop it and reset the offset.
    if (topmost_chunk_offset_ >= topmost.length) {
      [pending_data_ removeObjectAtIndex:0];
      topmost_chunk_offset_ = 0;
    }

    if (block_result.has_value()) {
      return *block_result;
    }
  }
  return MOJO_RESULT_OK;
}

}  // namespace updater
