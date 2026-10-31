// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ALOUD_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ALOUD_APP_MODEL_H_

#include <map>
#include <memory>
#include <string>

#include "base/metrics/single_sample_metrics.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"

class ReadAnythingReadAloudAppModelTest;

// A class that holds state related to Read Aloud for the
// ReadAnythingAppController for the Read Anything WebUI app.
class ReadAloudAppModel {
 public:
  // Enum for logging when speech is stopped and why.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ReadAloudStopSource)
  enum class ReadAloudStopSource {
    kButton = 0,
    kKeyboardShortcut = 1,
    kCloseReadingMode = 2,
    kCloseTabOrWindow = 3,
    kReloadPage = 4,
    kChangePage = 5,
    kEngineInterrupt = 6,
    kEngineError = 7,
    kFinishContent = 8,
    kLockChromeosDevice = 9,
    kUnexpectedUpdateContent = 10,

    kMinValue = kButton,
    kMaxValue = kUnexpectedUpdateContent,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingSpeechStopSource)

  // LINT.IfChange(ReadAnythingPlaybackContext)
  enum class ReadAnythingPlaybackContext {
    kSidePanel = 0,
    kImmersive = 1,

    kMinValue = kSidePanel,
    kMaxValue = kImmersive,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingPlaybackContext)

  static constexpr char kSpeechStopSourceHistogramName[] =
      "Accessibility.ReadAnything.SpeechStopSource";
  static constexpr char kAudioStartTimeFailureHistogramName[] =
      "Accessibility.ReadAnything.AudioStartTime.Failure";
  static constexpr char kAudioStartTimeSuccessHistogramName[] =
      "Accessibility.ReadAnything.AudioStartTime.Success";
  static constexpr char kPlaybackContextHistogramName[] =
      "Accessibility.ReadAnything.ReadAloud.PlaybackContext";

  ReadAloudAppModel();
  ~ReadAloudAppModel();
  ReadAloudAppModel(const ReadAloudAppModel& other) = delete;
  ReadAloudAppModel& operator=(const ReadAloudAppModel&) = delete;

  bool speech_playing() const { return speech_playing_; }
  void SetSpeechPlaying(bool is_playing);
  ReadAnythingPlaybackContext current_session_context_for_testing() const {
    return current_session_context_for_testing_;
  }

  bool audio_currently_playing() const { return audio_currently_playing_; }
  void SetAudioCurrentlyPlaying(bool is_playing);
  double speech_rate() const { return speech_rate_; }
  void set_speech_rate(double rate) { speech_rate_ = rate; }
  const base::ListValue& languages_enabled_in_pref() const {
    return languages_enabled_in_pref_;
  }
  void SetLanguageEnabled(const std::string& lang, bool enabled);
  const base::DictValue& voices() const { return voices_; }
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
  void set_default_language_code(const std::string& code) {
    default_language_code_ = code;
  }

  bool IsHighlightOn();
  void OnSettingsRestoredFromPrefs(
      double speech_rate,
      base::ListValue* languages_enabled_in_pref,
      base::DictValue* voices,
      read_anything::mojom::HighlightGranularity granularity);

  // Get the dependency parsing model for this renderer process.
  base::SequenceBound<DependencyParserModel>& GetDependencyParserModel();

  void ResetAndLogSingleSampleMetrics();

  // Updates the session count for the given metric name using
  // SingleSampleMetric. These are then logged once on destruction.
  void IncrementMetric(const std::string& metric_name);

  void LogSpeechStop(ReadAloudStopSource source);
  void LogPlaybackContext(ReadAnythingPlaybackContext context);

 private:
  friend ReadAnythingReadAloudAppModelTest;

  void LogAudioDelay(bool success);

  // Whether Read Aloud speech was initiated. Audio may or may not have actually
  // started output.
  bool speech_playing_ = false;
  // Whether audio for Read aloud is actually playing.
  bool audio_currently_playing_ = false;

  // The current speech rate for reading aloud.
  double speech_rate_ = 1.0;

  // The languages that the user has enabled for reading aloud.
  base::ListValue languages_enabled_in_pref_;

  // The user's preferred voices. Maps from a language to the last chosen
  // voice for that language.
  base::DictValue voices_;

  // The current granularity being used for the reading highlight.
  int highlight_granularity_ =
      (int)read_anything::mojom::HighlightGranularity::kDefaultValue;

  // The default language code, used as a fallback in case the page language
  // is invalid. It's not guaranteed that default_language_code_ will always
  // be valid, but as it is tied to the browser language, it is likely more
  // stable.
  std::string default_language_code_ = "en";

  // Metrics for logging. Any metric that we want to track 0-counts of should
  // be initialized here.
  const int min_sample = 0;
  const int max_sample = 1000;
  const uint32_t buckets = 50;
  std::map<std::string, int64_t> metric_to_count_map_ = {
      {"Accessibility.ReadAnything.ReadAloudNextButtonSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPauseSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPlaySessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPlayFromLineFocusSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPlayFromSelectionSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPreviousButtonSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloud.VoiceLanguageChange", 0},
  };
  std::map<std::string, std::unique_ptr<base::SingleSampleMetric>>
      metric_to_single_sample_;

  // The time when the speech becomes active.
  base::TimeTicks speech_active_time_ms_;

  // NOTE: This context is set at playback start. It can be used in the future
  // to segment duration metrics (SpeechPlaybackSession) or errors by surface
  // (Side Panel vs. Immersive).
  ReadAnythingPlaybackContext current_session_context_for_testing_ =
      ReadAnythingPlaybackContext::kSidePanel;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ALOUD_APP_MODEL_H_
