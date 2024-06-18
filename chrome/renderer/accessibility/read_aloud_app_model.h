// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_

#include "base/values.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
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
  const base::Value::List& languages_enabled_in_pref() const {
    return languages_enabled_in_pref_;
  }
  void SetLanguageEnabled(const std::string& lang, bool enabled);
  const base::Value::Dict& voices() const { return voices_; }
  void SetVoice(const std::string& voice, const std::string& lang) {
    voices_.Set(lang, voice);
  }
  int highlight_granularity() const { return highlight_granularity_; }
  void set_highlight_granularity(int granularity) {
    highlight_granularity_ = granularity;
  }
  const std::string& default_language_code() const {
    return default_language_code_;
  }
  void set_default_language_code(const std::string code) {
    default_language_code_ = code;
  }

  bool IsHighlightOn();
  void OnSettingsRestoredFromPrefs(
      double speech_rate,
      base::Value::List* languages_enabled_in_pref,
      base::Value::Dict* voices,
      read_anything::mojom::HighlightGranularity granularity);

 private:
  // Whether Read Aloud speech is currently playing or not.
  bool speech_playing_ = false;

  // The current speech rate for reading aloud.
  double speech_rate_ = kReadAnythingDefaultSpeechRate;

  // The languages that the user has enabled for reading aloud.
  base::Value::List languages_enabled_in_pref_;

  // The user's preferred voices. Maps from a language to the last chosen
  // voice for that language.
  base::Value::Dict voices_;

  // The current granularity being used for the reading highlight.
  int highlight_granularity_ =
      (int)read_anything::mojom::HighlightGranularity::kDefaultValue;

  // The default language code, used as a fallback in case the page language
  // is invalid. It's not guaranteed that default_language_code_ will always
  // be valid, but as it is tied to the browser language, it is likely more
  // stable.
  std::string default_language_code_ = "en";
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_
