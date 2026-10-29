// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TTC_TTC_VOICE_PLATE_H_
#define CHROME_BROWSER_UI_VIEWS_TTC_TTC_VOICE_PLATE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/voice/voice_plate.h"
#include "chrome/browser/ui/views/voice/waveform_mode.h"
#include "chrome/browser/ui/views/voice/waveform_view.h"
#include "ui/base/interaction/element_identifier.h"

namespace views {
class View;
}  // namespace views

namespace ttc {

// The voice plate of TTC. A voice plate is a toast-like bubble at the top of
// the browser window used in Voice Experiences (see `voice::VoicePlate`).
class TtcVoicePlate : public voice::VoicePlate {
 public:
  TtcVoicePlate(views::View* anchor_view,
                base::RepeatingClosure close_callback);
  TtcVoicePlate(const TtcVoicePlate&) = delete;
  TtcVoicePlate& operator=(const TtcVoicePlate&) = delete;
  ~TtcVoicePlate() override;

  // TODO(danielmendz): Take a TTC Ui State instead and convert it to
  // WaveformMode.
  void SetMode(voice::WaveformMode mode);
  voice::WaveformMode mode() const { return waveform_view_->mode(); }
  void UpdateAudioLevel(float audio_level) override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kViewElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWaveformElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonElementId);

 private:
  raw_ptr<voice::WaveformView> waveform_view_ = nullptr;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_VIEWS_TTC_TTC_VOICE_PLATE_H_
