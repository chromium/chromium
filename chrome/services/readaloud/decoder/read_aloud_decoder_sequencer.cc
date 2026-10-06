// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoder/read_aloud_decoder_sequencer.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
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

void ReadAloudDecoderSequencer::SetNextChunkToDecode(
    uint32_t chunk_index,
    uint32_t min_global_char_offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  is_decoding_ = false;
  next_chunk_to_decode_ = chunk_index;
  min_global_char_offset_ = min_global_char_offset;
  sought_to_end_ = prefetch_manager_->GetTimelineChunkCount() > 0 &&
                   chunk_index >= prefetch_manager_->GetTimelineChunkCount();
  ReplenishBuffer();
}

void ReadAloudDecoderSequencer::SetPumpStatusCallback(
    PumpStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pump_status_callback_ = std::move(callback);
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
  is_decoding_ = false;
  next_chunk_to_decode_ = 0;
  min_global_char_offset_ = 0;
  produced_audio_ = false;
  sought_to_end_ = false;
  pump_timer_.Stop();
}

void ReadAloudDecoderSequencer::ReplenishBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!audio_segment_queue_ || is_replenishing_) {
    return;
  }

  base::AutoReset<bool> replenishing_guard(&is_replenishing_, true);

  // Step 1: In-Order Decoding & Fast-Skipping Failed Chunks.
  // Advances next_chunk_to_decode_ past any failed/corrupt chunks in cache.
  while (!is_decoding_ &&
         next_chunk_to_decode_ < prefetch_manager_->GetTimelineChunkCount() &&
         audio_segment_queue_->GetBufferedDuration() <
             kMaxDecodedAudioDuration) {
    const uint32_t chunk_index = next_chunk_to_decode_;
    const CachedCompressedSegment* cached =
        prefetch_manager_->GetCachedSegment(chunk_index);
    if (!cached) {
      break;  // Chunk response not received yet; wait here.
    }

    if (cached->status == SynthesisResultStatus::kSuccess &&
        cached->opus_buffer) {
      is_decoding_ = true;
      const uint64_t sequence_id = prefetch_manager_->GetCurrentSequenceId();
      decoder_helper_->DecodeAndSlice(
          cached->opus_buffer, cached->timings,
          base::BindOnce(&ReadAloudDecoderSequencer::OnAudioDecoded,
                         weak_ptr_factory_.GetWeakPtr(), sequence_id,
                         chunk_index));
      break;  // Started decoding valid audio segment.
    }

    // Chunk synthesis failed or returned empty audio; log warning and skip.
    LOG(WARNING) << "ReadAloud: Skipping chunk " << chunk_index
                 << " due to synthesis failure (status: "
                 << static_cast<int>(cached->status) << ")";
    min_global_char_offset_ = 0;
    next_chunk_to_decode_++;
  }

  // Step 2: Network Lookahead.
  // Evaluates lookahead FROM THE UPDATED next_chunk_to_decode_. If any chunks
  // failed in Step 1, this immediately extends the prefetch window to request
  // downstream replacement chunks!
  if (next_chunk_to_decode_ < prefetch_manager_->GetTimelineChunkCount()) {
    std::vector<uint32_t> required_chunks =
        prefetch_manager_->GetRequiredPrefetchChunks(
            next_chunk_to_decode_, audio_segment_queue_->GetBufferedDuration());
    for (uint32_t idx : required_chunks) {
      prefetch_manager_->SchedulePrefetch(idx);
    }
  }

  // Step 3: Report the renderer-facing pump status. Note that
  // `is_replenishing_` is still set here, so a reentrant ReplenishBuffer() from
  // the callback is safely suppressed.
  NotifyPumpStatus();
}

ReadAloudDecoderSequencer::PumpStatus
ReadAloudDecoderSequencer::EvaluatePumpStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!audio_segment_queue_->GetBufferedDuration().is_zero()) {
    return PumpStatus::kFlowing;
  }
  if (next_chunk_to_decode_ < prefetch_manager_->GetTimelineChunkCount()) {
    return PumpStatus::kStarved;
  }
  // The timeline is fully consumed. Whether that is a normal end of document
  // or a total synthesis/decode washout depends on whether any audio was
  // decoded or if the user explicitly sought to the end of the timeline.
  return (produced_audio_ || sought_to_end_) ? PumpStatus::kDrained
                                             : PumpStatus::kFailed;
}

void ReadAloudDecoderSequencer::NotifyPumpStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pump_status_callback_ || !pump_timer_.IsRunning() ||
      !audio_segment_queue_) {
    return;
  }
  pump_status_callback_.Run(EvaluatePumpStatus());
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
  const uint32_t min_offset = min_global_char_offset_;
  min_global_char_offset_ = 0;
  bool skipped_by_min_offset = false;
  for (auto& segment : decoded_segments) {
    if (!segment) {
      continue;
    }
    if (min_offset > 0 && !segment->word_timings().empty()) {
      const WordTiming& timing = segment->word_timings().front();
      // Skip words that end at or before `min_offset`, while preserving a
      // zero-length word timing anchored exactly at `min_offset`
      // (`start_character_offset == end_character_offset == min_offset`).
      if (timing.end_character_offset <= min_offset &&
          timing.start_character_offset < min_offset) {
        skipped_by_min_offset = true;
        continue;
      }
    }
    if (audio_segment_queue_->Push(std::move(segment))) {
      produced_audio_ = true;
    }
  }
  if (skipped_by_min_offset) {
    produced_audio_ = true;
  }
  next_chunk_to_decode_++;
  ReplenishBuffer();
}

}  // namespace readaloud
