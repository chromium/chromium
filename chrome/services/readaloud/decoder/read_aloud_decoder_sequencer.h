// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_DECODER_READ_ALOUD_DECODER_SEQUENCER_H_
#define CHROME_SERVICES_READALOUD_DECODER_READ_ALOUD_DECODER_SEQUENCER_H_

#include <cstdint>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"

namespace readaloud {

class AudioSegmentQueue;
class OpusDecoderHelper;
class PrefetchManager;

// Coordinates the demand-driven decoding of speech synthesis audio chunks
// in strictly sequential order from `PrefetchManager` to `AudioSegmentQueue`.
class ReadAloudDecoderSequencer {
 public:
  ReadAloudDecoderSequencer(PrefetchManager* prefetch_manager,
                            OpusDecoderHelper* decoder_helper,
                            AudioSegmentQueue* audio_segment_queue = nullptr);

  ReadAloudDecoderSequencer(const ReadAloudDecoderSequencer&) = delete;
  ReadAloudDecoderSequencer& operator=(const ReadAloudDecoderSequencer&) =
      delete;

  ~ReadAloudDecoderSequencer();

  // Starts the repeating watchdog timer to continuously pump decoded segments.
  void StartPumping();

  // Stops the watchdog timer.
  void StopPumping();

  // Inspects the next in-order chunk from PrefetchManager and initiates
  // decoding if available in cache, or schedules network prefetching on cache
  // misses. Halts when AudioSegmentQueue duration is at or above
  // kMaxDecodedAudioDuration, when already decoding, or when all timeline
  // chunks have been processed.
  void ReplenishBuffer();

  // Resets decoding state (clears in-progress flags, resets
  // next_chunk_to_decode_ to 0).
  void Reset();

  // Sets or updates the target AudioSegmentQueue (e.g. after audio
  // initialization or teardown).
  void SetAudioQueue(AudioSegmentQueue* audio_segment_queue);

  // Updates the next sequential chunk index to decode (e.g. after seek
  // operations).
  void SetNextChunkToDecode(uint32_t chunk_index);

  uint32_t next_chunk_to_decode() const { return next_chunk_to_decode_; }
  bool is_decoding() const { return is_decoding_; }

 private:
  void OnAudioDecoded(
      uint64_t sequence_id,
      uint32_t chunk_index,
      std::vector<scoped_refptr<DecodedAudioSegment>> decoded_segments);

  const raw_ptr<PrefetchManager> prefetch_manager_;
  const raw_ptr<OpusDecoderHelper> decoder_helper_;
  raw_ptr<AudioSegmentQueue> audio_segment_queue_ = nullptr;

  uint32_t next_chunk_to_decode_ = 0;
  bool is_decoding_ = false;
  bool is_replenishing_ = false;

  base::RepeatingTimer pump_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ReadAloudDecoderSequencer> weak_ptr_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_DECODER_READ_ALOUD_DECODER_SEQUENCER_H_
