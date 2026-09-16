// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_NET_CHUNK_QUEUE_H_
#define CHROME_UPDATER_NET_CHUNK_QUEUE_H_

#import <Foundation/Foundation.h>
#include <stdint.h>

#include <cstddef>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "mojo/public/c/system/types.h"

namespace updater {

// ChunkQueue manages a queue of NSData* blocks. Data is consumed from the queue
// chunk-by-chunk with support for partial chunk consumption. ChunkQueue avoids
// copying or reallocating the underlying data. ChunkQueue must be created,
// used, and destroyed on a single sequence.
class ChunkQueue {
 public:
  ChunkQueue();
  ~ChunkQueue();

  ChunkQueue(const ChunkQueue&) = delete;
  ChunkQueue& operator=(const ChunkQueue&) = delete;

  // Returns true if there is no data in the queue.
  bool empty() const;

  // Adds a chunk of data to the back of the queue.
  void Push(NSData* data);

  // Invokes `write_callback` with contiguous slices of data from the front of
  // the queue until all data is consumed, the callback blocks (returns
  // MOJO_RESULT_SHOULD_WAIT), or an error occurs. Advances the internal read
  // offset by the number of bytes successfully consumed.
  //
  // The callback is only invoked synchronously during the execution of
  // Consume(); it is never stored or invoked after Consume() returns.
  //
  // The provided `write_callback` should return:
  // - MOJO_RESULT_OK: If it wrote some or all of the slice. It must set
  //   `bytes_written` to the number of bytes consumed. If `bytes_written` is
  //   less than the slice size, iteration will halt and Consume() will return
  //   MOJO_RESULT_SHOULD_WAIT.
  // - MOJO_RESULT_SHOULD_WAIT: If the destination is full and no bytes could be
  //   written. It must set `bytes_written` to 0.
  // - Any other MojoResult error code: On terminal failure.
  //
  // Returns:
  // - MOJO_RESULT_OK if all data was successfully consumed.
  // - MOJO_RESULT_SHOULD_WAIT if the callback is blocked.
  // - Any other MojoResult error returned by the callback.
  MojoResult Consume(
      base::RepeatingCallback<MojoResult(base::span<const uint8_t> slice,
                                         size_t& bytes_written)>
          write_callback);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  NSMutableArray<NSData*>* pending_data_;
  // The byte offset within the topmost chunk (pending_data_[0]) that has
  // already been consumed. Resuming `Consume` calls start from this offset.
  // Tracking an offset is preferred over slicing NSData via `subdataWithRange`
  // as it could cause non-contigious NSData implementations to be copied.
  size_t topmost_chunk_offset_ = 0;
};

}  // namespace updater

#endif  // CHROME_UPDATER_NET_CHUNK_QUEUE_H_
