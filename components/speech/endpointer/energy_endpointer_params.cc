// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/speech/endpointer/energy_endpointer_params.h"

namespace speech {

EnergyEndpointerParams::EnergyEndpointerParams() {
  SetDefaults();
}

void EnergyEndpointerParams::SetDefaults() {
  frame_period_ = 0.01f;
  frame_duration_ = 0.01f;
  endpoint_margin_ = 0.2f;
  onset_window_ = 0.15f;
  speech_on_window_ = 0.4f;
  offset_window_ = 0.15f;
  onset_detect_dur_ = 0.09f;
  onset_confirm_dur_ = 0.075f;
  on_maintain_dur_ = 0.10f;
  offset_confirm_dur_ = 0.12f;
  decision_threshold_ = 150.0f;
  min_decision_threshold_ = 50.0f;
  fast_update_dur_ = 0.2f;
  sample_rate_ = 8000.0f;
  min_fundamental_frequency_ = 57.143f;
  max_fundamental_frequency_ = 400.0f;
  contamination_rejection_period_ = 0.25f;
}

void EnergyEndpointerParams::operator=(const EnergyEndpointerParams& source) {
  frame_period_ = source.frame_period();
  frame_duration_ = source.frame_duration();
  endpoint_margin_ = source.endpoint_margin();
  onset_window_ = source.onset_window();
  speech_on_window_ = source.speech_on_window();
  offset_window_ = source.offset_window();
  onset_detect_dur_ = source.onset_detect_dur();
  onset_confirm_dur_ = source.onset_confirm_dur();
  on_maintain_dur_ = source.on_maintain_dur();
  offset_confirm_dur_ = source.offset_confirm_dur();
  decision_threshold_ = source.decision_threshold();
  min_decision_threshold_ = source.min_decision_threshold();
  fast_update_dur_ = source.fast_update_dur();
  sample_rate_ = source.sample_rate();
  min_fundamental_frequency_ = source.min_fundamental_frequency();
  max_fundamental_frequency_ = source.max_fundamental_frequency();
  contamination_rejection_period_ = source.contamination_rejection_period();
}

}  //  namespace speech
