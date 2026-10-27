// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_UI_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_UI_STATE_H_

#include "chrome/browser/ui/views/voice/waveform_mode.h"

namespace dictation {

enum class UiState {
  kInactive,
  kInitializing,
  kTranscribing,
  kFinalizing,
};

voice::WaveformMode ToWaveformMode(UiState state);

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_UI_STATE_H_
