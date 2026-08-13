// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MODE_SCHEDULER_H_
#define CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MODE_SCHEDULER_H_

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/chunking/text_chunker.h"

namespace readaloud {

// Manages dynamic hysteresis state transitions between speed and quality
// prefetch modes based on buffered audio duration to prevent mode flapping.
//
// Threading & Sequence Safety:
// PrefetchModeScheduler lives exclusively on the Utility Process Main Sequence
// (Mojo IPC sequence). All state transitions and duration evaluations
// (UpdateMode, Reset, GetChunkingMode) must be executed on this sequence,
// enforced via `SEQUENCE_CHECKER`. It does not execute on the real-time Audio
// Thread or background decoder thread.
//
// Lifecycle:
// - Created on demand within PrefetchManager on the Utility Main Sequence in
//   default `kSpeed` mode.
// - State is reset back to `kSpeed` upon document text changes (SetTextContent)
//   or session aborts/resets (ResetSession).
// - Sequence-affine: must be created and invoked on the same sequence.
//
// Hysteresis State Machine:
// - Default Mode: Starts in `ChunkingMode::kSpeed` (target prefetch duration:
//   15s).
// - Upgrade Threshold: Transitions from `kSpeed` to `kQuality` when
//   `current_buffered_duration >= 15s` (`kAudioBufferPrefetchWatermark`).
// - Hysteresis Zone Retention: Retains `kQuality` when duration fluctuates
//   between 5s and 15s to prevent oscillation.
// - Downgrade Threshold: Transitions back from `kQuality` to `kSpeed` only if
//   duration drops below 5s (`kAudioBufferMinDuration`).
class PrefetchModeScheduler {
 public:
  PrefetchModeScheduler();
  PrefetchModeScheduler(const PrefetchModeScheduler&) = delete;
  PrefetchModeScheduler& operator=(const PrefetchModeScheduler&) = delete;
  ~PrefetchModeScheduler();

  // Evaluates the current buffered audio duration and returns the updated
  // prefetch mode after applying hysteresis transition thresholds.
  //
  // @param current_buffered_duration Total duration of decoded audio available.
  // @return Updated `ChunkingMode` (`kSpeed` or `kQuality`).
  ChunkingMode UpdateMode(base::TimeDelta current_buffered_duration);

  // Resets the scheduler back to default startup state
  // (`ChunkingMode::kSpeed`). Should be invoked when document sessions reset or
  // timelines are cleared.
  void Reset();

  // Returns the active chunking mode (`kSpeed` or `kQuality`).
  ChunkingMode GetChunkingMode() const;

  // Returns the target prefetch audio duration for the current mode:
  // - `kSpeed`: 15s (`kAudioBufferPrefetchWatermark`).
  // - `kQuality`: 50s (`kMaxDecodedAudioDuration`).
  base::TimeDelta GetTargetPrefetchDuration() const;

 private:
  ChunkingMode current_mode_ = ChunkingMode::kSpeed;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MODE_SCHEDULER_H_
