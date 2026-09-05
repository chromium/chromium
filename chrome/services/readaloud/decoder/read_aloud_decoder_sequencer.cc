// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoder/read_aloud_decoder_sequencer.h"

#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/services/readaloud/audio_segment_queue.h"
#include "chrome/services/readaloud/decoder/opus_decoder_helper.h"
#include "chrome/services/readaloud/prefetch/prefetch_manager.h"

namespace readaloud {

ReadAloudDecoderSequencer::ReadAloudDecoderSequencer(
    PrefetchManager* prefetch_manager,
    OpusDecoderHelper* decoder_helper,
    AudioSegmentQueue* audio_segment_queue)
    : prefetch_manager_(prefetch_manager),
      decoder_helper_(decoder_helper),
      audio_segment_queue_(audio_segment_queue) {
  DCHECK(prefetch_manager_);
  DCHECK(decoder_helper_);
}

ReadAloudDecoderSequencer::~ReadAloudDecoderSequencer() = default;

void ReadAloudDecoderSequencer::SetAudioQueue(
    AudioSegmentQueue* audio_segment_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_segment_queue_ = audio_segment_queue;
}

void ReadAloudDecoderSequencer::SetNextChunkToDecode(uint32_t chunk_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  next_chunk_to_decode_ = chunk_index;
}

void ReadAloudDecoderSequencer::StartPumping() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pump_timer_.IsRunning()) {
    // TODO(b/524284196): Replace or supplement this polling timer with
    // event-driven triggers when AV sync is implemented.
    pump_timer_.Start(
        FROM_HERE, base::Milliseconds(250),
        base::BindRepeating(&ReadAloudDecoderSequencer::ReplenishBuffer,
                            base::Unretained(this)));
  }
  ReplenishBuffer();
}

void ReadAloudDecoderSequencer::StopPumping() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pump_timer_.Stop();
}

void ReadAloudDecoderSequencer::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  pump_timer_.Stop();
  next_chunk_to_decode_ = 0;
  is_decoding_ = false;
}

void ReadAloudDecoderSequencer::ReplenishBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!audio_segment_queue_ || is_replenishing_) {
    return;
  }

  base::AutoReset<bool> replenishing_guard(&is_replenishing_, true);

  while (!is_decoding_ &&
         next_chunk_to_decode_ < prefetch_manager_->GetTimelineChunkCount() &&
         audio_segment_queue_->GetBufferedDuration() <
             kMaxDecodedAudioDuration) {
    const uint32_t chunk_index = next_chunk_to_decode_;

    // Step 1: Network Lookahead.
    // Calculate how many future chunks we need to hit the prefetch watermark
    // and eagerly schedule them to utilize parallel network connections.
    std::vector<uint32_t> required_chunks =
        prefetch_manager_->GetRequiredPrefetchChunks(
            chunk_index, audio_segment_queue_->GetBufferedDuration());
    for (uint32_t idx : required_chunks) {
      prefetch_manager_->SchedulePrefetch(idx);
    }

    // Step 2: In-Order Decoding.
    // If the next sequential chunk is fully downloaded and cached, begin
    // decoding it. Otherwise, we naturally block and wait for it to arrive.
    const CachedCompressedSegment* cached =
        prefetch_manager_->GetCachedSegment(chunk_index);
    if (!cached) {
      break;
    }

    if (cached->opus_buffer) {
      is_decoding_ = true;
      const uint64_t sequence_id = prefetch_manager_->GetCurrentSequenceId();
      decoder_helper_->DecodeAndSlice(
          cached->opus_buffer, cached->timings,
          base::BindOnce(&ReadAloudDecoderSequencer::OnAudioDecoded,
                         weak_ptr_factory_.GetWeakPtr(), sequence_id,
                         chunk_index));
      break;
    }

    // Chunk synthesis failed or returned empty audio; skip past it iteratively.
    next_chunk_to_decode_++;
  }
}

void ReadAloudDecoderSequencer::OnAudioDecoded(
    uint64_t sequence_id,
    uint32_t chunk_index,
    std::vector<scoped_refptr<DecodedAudioSegment>> decoded_segments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_decoding_ = false;
  if (sequence_id != prefetch_manager_->GetCurrentSequenceId()) {
    return;
  }

  if (!audio_segment_queue_) {
    return;
  }
  for (auto& segment : decoded_segments) {
    if (segment) {
      std::ignore = audio_segment_queue_->Push(std::move(segment));
    }
  }
  next_chunk_to_decode_++;
  ReplenishBuffer();
}

}  // namespace readaloud
