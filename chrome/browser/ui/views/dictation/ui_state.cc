// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/ui_state.h"

namespace dictation {

voice::WaveformMode ToWaveformMode(UiState state) {
  using voice::WaveformMode;
  switch (state) {
    case UiState::kInactive:
      return WaveformMode::kIdle;
    case UiState::kInitializing:
      return WaveformMode::kWaiting;
    case UiState::kTranscribing:
      return WaveformMode::kLive;
    case UiState::kFinalizing:
      return WaveformMode::kProcessing;
  }
}

}  // namespace dictation
