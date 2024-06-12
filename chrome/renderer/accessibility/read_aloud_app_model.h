// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_

#include "chrome/common/accessibility/read_anything_constants.h"

// A class that holds state related to Read Aloud for the
// ReadAnythingAppController for the Read Anything WebUI app.
class ReadAloudAppModel {
 public:
  ReadAloudAppModel();
  ~ReadAloudAppModel();
  ReadAloudAppModel(const ReadAloudAppModel& other) = delete;
  ReadAloudAppModel& operator=(const ReadAloudAppModel&) = delete;

  bool speech_playing() { return speech_playing_; }
  void set_speech_playing(bool is_playing) { speech_playing_ = is_playing; }
  double speech_rate() const { return speech_rate_; }
  void set_speech_rate(double rate) { speech_rate_ = rate; }

  void OnSettingsRestoredFromPrefs(double speech_rate);

 private:
  // Whether Read Aloud speech is currently playing or not.
  bool speech_playing_ = false;

  // The current speech rate for reading aloud.
  double speech_rate_ = kReadAnythingDefaultSpeechRate;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_
