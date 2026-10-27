// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VOICE_WAVEFORM_MODE_H_
#define CHROME_BROWSER_UI_VIEWS_VOICE_WAVEFORM_MODE_H_

namespace voice {

// What a `WaveformView` draws.
enum class WaveformMode {
  // Resting dots. Clears the audio history, and collapses the view if
  // WaveformView::collapse_when_idle_ is true.
  kIdle,
  // Resting dots.
  kWaiting,
  // Bars driven by the mic volume.
  kLive,
  // A wave traveling across resting dots.
  kProcessing,
};

}  // namespace voice

#endif  // CHROME_BROWSER_UI_VIEWS_VOICE_WAVEFORM_MODE_H_
