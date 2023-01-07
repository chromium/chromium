// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_METRICS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_METRICS_H_

#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace quick_answers {

// The different TTS engine events that are received by the quick
// answers utterance event delegate.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Also remember to update the QuickAnswersTextToSpeechEngineEvent enum
// listing in tools/metrics/histograms/enums.xml.
enum class TtsEngineEvent {
  TTS_EVENT_START = 0,
  TTS_EVENT_END = 1,
  TTS_EVENT_ERROR = 2,
  TTS_EVENT_OTHER = 3,

  kMaxValue = TTS_EVENT_OTHER
};

// Enumeration of dictionary intent source type.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Also remember to update the QuickAnswersDictionaryIntentSource enum
// listing in tools/metrics/histograms/enums.xml.
enum class DictionaryIntentSource {
  kTextClassifier = 0,
  kHunspell = 1,

  kMaxValue = kHunspell,
};

// Record the status of loading quick answers with status and duration.
void RecordLoadingStatus(LoadStatus status, const base::TimeDelta duration);

// Record loading result with result type and network latency.
void RecordResult(ResultType result_type, const base::TimeDelta duration);

// Record quick answers user clicks with result type and duration between result
// fetch finish and user clicks.
void RecordClick(ResultType result_type, const base::TimeDelta duration);

// Record selected text length to learn about usage pattern.
void RecordSelectedTextLength(int length);

// Record selected text length of requests sent out to learn about usage
// pattern.
void RecordRequestTextLength(IntentType intent_type, int length);

// Record active impression with result type and impression duration.
void RecordActiveImpression(ResultType result_type,
                            const base::TimeDelta duration);

// Record the intent generated on-device.
void RecordIntentType(IntentType intent_type);

// Record the intent type when network error occurs.
void RecordNetworkError(IntentType intent_type, int response_code);

// Record the TTS engine event types as they occur in quick answers.
void RecordTtsEngineEvent(TtsEngineEvent event);

// Record the source type of dictionary intent.
void RecordDictionaryIntentSource(DictionaryIntentSource source);

// Record the query language of dictionary intent.
void RecordDictionaryIntentLanguage(const std::string& language);

// Record the feature enabled status when the first user session starts.
void RecordFeatureEnabled(bool enabled);

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_METRICS_H_
