// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/language/core/browser/language_usage_metrics.h"

namespace quick_answers {

namespace {

const char kQuickAnswerActiveImpression[] = "QuickAnswers.ActiveImpression";
const char kQuickAnswerClick[] = "QuickAnswers.Click";
const char kQuickAnswerResult[] = "QuickAnswers.Result";
const char kQuickAnswerIntent[] = "QuickAnswers.Intent";
const char kQuickAnswerLoadingStatus[] = "QuickAnswers.Loading.Status";
const char kQuickAnswerLoadingDuration[] = "QuickAnswers.Loading.Duration";
const char kQuickAnswerSelectedContentLength[] =
    "QuickAnswers.SelectedContent.Length";
const char kQuickAnswersRequestTextLength[] = "QuickAnswers.RequestTextLength";
const char kQuickAnswersTtsEngineEvent[] =
    "QuickAnswers.TextToSpeech.EngineEvent";
const char kQuickAnswersDictionaryIntentSource[] =
    "QuickAnswers.DictionaryIntent.Source";
const char kQuickAnswersDictionaryIntentLanguage[] =
    "QuickAnswers.DictionaryIntent.Language";
const char kQuickAnswersNetworkError[] = "QuickAnswers.NetworkError.IntentType";
const char kQuickAnswersNetworkResponseCode[] =
    "QuickAnswers.NetworkError.ResponseCode";
const char kQuickAnswerFeatureEnabled[] = "QuickAnswers.FeatureEnabled";

const char kDurationSuffix[] = ".Duration";
const char kDefinitionSuffix[] = ".Definition";
const char kTranslationSuffix[] = ".Translation";
const char kUnitConversionSuffix[] = ".UnitConversion";

std::string ResultTypeToString(ResultType result_type) {
  switch (result_type) {
    case ResultType::kNoResult:
      return "NoResult";
    case ResultType::kDefinitionResult:
      return "Definition";
    case ResultType::kTranslationResult:
      return "Translation";
    case ResultType::kUnitConversionResult:
      return "UnitConversion";
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid ResultType.";
      return ".Unknown";
  }
}

void RecordTypeAndDuration(const std::string& prefix,
                           ResultType result_type,
                           const base::TimeDelta duration,
                           bool is_medium_bucketization) {
  // Record by result type.
  base::UmaHistogramSparse(prefix, static_cast<int>(result_type));

  const std::string duration_histogram = prefix + kDurationSuffix;
  const std::string result_type_histogram_name =
      base::StringPrintf("%s.%s", duration_histogram.c_str(),
                         ResultTypeToString(result_type).c_str());
  // Record sliced by duration and result type.
  if (is_medium_bucketization) {
    base::UmaHistogramMediumTimes(duration_histogram, duration);
    base::UmaHistogramMediumTimes(result_type_histogram_name.c_str(), duration);
  } else {
    base::UmaHistogramTimes(duration_histogram, duration);
    base::UmaHistogramTimes(result_type_histogram_name.c_str(), duration);
  }
}

}  // namespace

void RecordResult(ResultType result_type, const base::TimeDelta duration) {
  RecordTypeAndDuration(kQuickAnswerResult, result_type, duration,
                        /*is_medium_bucketization=*/false);
}

void RecordLoadingStatus(LoadStatus status, const base::TimeDelta duration) {
  base::UmaHistogramEnumeration(kQuickAnswerLoadingStatus, status);
  base::UmaHistogramTimes(kQuickAnswerLoadingDuration, duration);
}

void RecordClick(ResultType result_type, const base::TimeDelta duration) {
  RecordTypeAndDuration(kQuickAnswerClick, result_type, duration,
                        /*is_medium_bucketization=*/true);
}

void RecordSelectedTextLength(int length) {
  base::UmaHistogramCounts1000(kQuickAnswerSelectedContentLength, length);
}

void RecordRequestTextLength(IntentType intent_type, int length) {
  std::string histogram_name = kQuickAnswersRequestTextLength;
  switch (intent_type) {
    case IntentType::kDictionary:
      histogram_name += kDefinitionSuffix;
      break;
    case IntentType::kTranslation:
      histogram_name += kTranslationSuffix;
      break;
    case IntentType::kUnit:
      histogram_name += kUnitConversionSuffix;
      break;
    case IntentType::kUnknown:
      return;
  }

  base::UmaHistogramCounts1000(histogram_name, length);
}

void RecordActiveImpression(ResultType result_type,
                            const base::TimeDelta duration) {
  RecordTypeAndDuration(kQuickAnswerActiveImpression, result_type, duration,
                        /*is_medium_bucketization=*/true);
}

void RecordIntentType(IntentType intent_type) {
  base::UmaHistogramEnumeration(kQuickAnswerIntent, intent_type);
}

void RecordNetworkError(IntentType intent_type, int response_code) {
  base::UmaHistogramEnumeration(kQuickAnswersNetworkError, intent_type);

  std::string histogram_name = kQuickAnswersNetworkResponseCode;
  switch (intent_type) {
    case IntentType::kDictionary:
      histogram_name += kDefinitionSuffix;
      break;
    case IntentType::kTranslation:
      histogram_name += kTranslationSuffix;
      break;
    case IntentType::kUnit:
      histogram_name += kUnitConversionSuffix;
      break;
    case IntentType::kUnknown:
      return;
  }

  base::UmaHistogramSparse(histogram_name, response_code);
}

void RecordTtsEngineEvent(TtsEngineEvent event) {
  base::UmaHistogramEnumeration(kQuickAnswersTtsEngineEvent, event);
}

void RecordDictionaryIntentSource(DictionaryIntentSource source) {
  base::UmaHistogramEnumeration(kQuickAnswersDictionaryIntentSource, source);
}

void RecordDictionaryIntentLanguage(const std::string& language) {
  base::UmaHistogramSparse(
      kQuickAnswersDictionaryIntentLanguage,
      language::LanguageUsageMetrics::ToLanguageCodeHash(language));
}

void RecordFeatureEnabled(bool enabled) {
  base::UmaHistogramBoolean(kQuickAnswerFeatureEnabled, enabled);
}

}  // namespace quick_answers
