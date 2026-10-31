// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_aloud_app_model.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"

namespace {

// Returns the dependency parser model for this renderer process.
// All access to this model is asynchronous and safely executed on a background
// sequenced task runner.
base::SequenceBound<DependencyParserModel>& GetDependencyParserModel_() {
  static base::NoDestructor<base::SequenceBound<DependencyParserModel>>
      instance;
  if (instance->is_null()) {
    *instance = base::SequenceBound<DependencyParserModel>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  }
  return *instance;
}

}  // namespace

ReadAloudAppModel::ReadAloudAppModel() {
  ResetAndLogSingleSampleMetrics();
}

ReadAloudAppModel::~ReadAloudAppModel() = default;

void ReadAloudAppModel::OnSettingsRestoredFromPrefs(
    double speech_rate,
    base::ListValue* languages_enabled_in_pref,
    base::DictValue* voices,
    read_anything::mojom::HighlightGranularity granularity) {
  speech_rate_ = speech_rate;
  languages_enabled_in_pref_ = languages_enabled_in_pref->Clone();
  voices_ = voices->Clone();
  highlight_granularity_ = static_cast<size_t>(granularity);
}

void ReadAloudAppModel::SetLanguageEnabled(const std::string& lang,
                                           bool enabled) {
  if (enabled) {
    languages_enabled_in_pref_.Append(lang);
  } else {
    languages_enabled_in_pref_.EraseValue(base::Value(lang));
  }
}

bool ReadAloudAppModel::IsHighlightOn() {
  return highlight_granularity_ !=
         static_cast<int>(read_anything::mojom::HighlightGranularity::kOff);
}

base::SequenceBound<DependencyParserModel>&
ReadAloudAppModel::GetDependencyParserModel() {
  return GetDependencyParserModel_();
}

void ReadAloudAppModel::ResetAndLogSingleSampleMetrics() {
  metric_to_single_sample_.clear();
  for (auto& [metric, count] : metric_to_count_map_) {
    count = 0;
    metric_to_single_sample_[metric] =
        base::SingleSampleMetricsFactory::Get()->CreateCustomCountsMetric(
            metric, min_sample, max_sample, buckets);
    metric_to_single_sample_[metric]->SetSample(min_sample);
  }
}

void ReadAloudAppModel::SetSpeechPlaying(bool is_playing) {
  if (is_playing) {
    speech_active_time_ms_ = base::TimeTicks::Now();
  }
  speech_playing_ = is_playing;
}

void ReadAloudAppModel::SetAudioCurrentlyPlaying(bool is_playing) {
  if (is_playing) {
    LogAudioDelay(/*success=*/true);
  }
  audio_currently_playing_ = is_playing;
}

void ReadAloudAppModel::IncrementMetric(const std::string& metric_name) {
  if (!metric_to_count_map_.contains(metric_name)) {
    return;
  }

  metric_to_count_map_[metric_name]++;
  // Update the count that will be logged on destruction.
  if (metric_to_single_sample_[metric_name]) {
    metric_to_single_sample_[metric_name]->SetSample(
        metric_to_count_map_[metric_name]);
  }
}

void ReadAloudAppModel::LogSpeechStop(ReadAloudStopSource source) {
  base::UmaHistogramEnumeration(kSpeechStopSourceHistogramName, source);
  // If speech started but audio is not playing yet when speech is stopped, log
  // the audio delay indicating that the user may have stopped speech because
  // audio wasn't starting.
  if (speech_playing_ && !audio_currently_playing_) {
    LogAudioDelay(/*success=*/false);
  }
}

void ReadAloudAppModel::LogAudioDelay(bool success) {
  const base::TimeDelta delay = base::TimeTicks::Now() - speech_active_time_ms_;
  if (success) {
    base::UmaHistogramLongTimes(kAudioStartTimeSuccessHistogramName, delay);
  } else {
    base::UmaHistogramLongTimes(kAudioStartTimeFailureHistogramName, delay);
  }
}

// Functionally acts as a session-start setter and logger. Safe because playback
// context remains constant and this is only called once per speech session.
void ReadAloudAppModel::LogPlaybackContext(
    ReadAnythingPlaybackContext context) {
  current_session_context_for_testing_ = context;
  base::UmaHistogramEnumeration(kPlaybackContextHistogramName, context);
}
