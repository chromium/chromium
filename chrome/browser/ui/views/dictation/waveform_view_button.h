// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_WAVEFORM_VIEW_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_WAVEFORM_VIEW_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/voice/waveform_mode.h"
#include "chrome/browser/ui/views/voice/waveform_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace dictation {

// Everything you love about WaveformView, now in a button!
class WaveformViewButton : public views::Button {
  METADATA_HEADER(WaveformViewButton, views::Button)

 public:
  WaveformViewButton(bool full_size, PressedCallback callback);
  WaveformViewButton(const WaveformViewButton&) = delete;
  WaveformViewButton& operator=(const WaveformViewButton&) = delete;
  ~WaveformViewButton() override;

  bool full_size() const { return waveform_view_->full_size(); }
  void SetMode(voice::WaveformMode mode) { waveform_view_->SetMode(mode); }
  voice::WaveformMode mode() const { return waveform_view_->mode(); }
  float audio_level_for_testing() const {
    return waveform_view_->audio_level_for_testing();
  }
  void SetAudioLevel(float level) { waveform_view_->SetAudioLevel(level); }

 private:
  raw_ptr<voice::WaveformView> waveform_view_ = nullptr;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_WAVEFORM_VIEW_BUTTON_H_
