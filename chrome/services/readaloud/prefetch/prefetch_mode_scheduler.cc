// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_mode_scheduler.h"

#include "chrome/common/readaloud/read_aloud_constants.h"

namespace readaloud {

PrefetchModeScheduler::PrefetchModeScheduler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

PrefetchModeScheduler::~PrefetchModeScheduler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ChunkingMode PrefetchModeScheduler::UpdateMode(
    base::TimeDelta current_buffered_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (current_mode_) {
    case ChunkingMode::kSpeed:
      if (current_buffered_duration >= kAudioBufferPrefetchWatermark) {
        current_mode_ = ChunkingMode::kQuality;
      }
      break;
    case ChunkingMode::kQuality:
      if (current_buffered_duration < kAudioBufferMinDuration) {
        current_mode_ = ChunkingMode::kSpeed;
      }
      break;
  }
  return current_mode_;
}

void PrefetchModeScheduler::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_mode_ = ChunkingMode::kSpeed;
}

ChunkingMode PrefetchModeScheduler::GetChunkingMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_mode_;
}

base::TimeDelta PrefetchModeScheduler::GetTargetPrefetchDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_mode_ == ChunkingMode::kSpeed ? kAudioBufferPrefetchWatermark
                                               : kMaxDecodedAudioDuration;
}

}  // namespace readaloud
