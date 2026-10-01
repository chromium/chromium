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

PrefetchMode PrefetchModeScheduler::UpdateMode(
    base::TimeDelta current_buffered_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (current_mode_) {
    case PrefetchMode::kSpeed:
      if (current_buffered_duration >= kAudioBufferPrefetchWatermark) {
        current_mode_ = PrefetchMode::kQuality;
      }
      break;
    case PrefetchMode::kQuality:
      if (current_buffered_duration < kAudioBufferMinDuration) {
        current_mode_ = PrefetchMode::kSpeed;
      }
      break;
  }
  return current_mode_;
}

void PrefetchModeScheduler::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_mode_ = PrefetchMode::kSpeed;
}

PrefetchMode PrefetchModeScheduler::GetPrefetchMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_mode_;
}

base::TimeDelta PrefetchModeScheduler::GetTargetPrefetchDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_mode_ == PrefetchMode::kSpeed ? kAudioBufferPrefetchWatermark
                                               : kMaxDecodedAudioDuration;
}

}  // namespace readaloud
