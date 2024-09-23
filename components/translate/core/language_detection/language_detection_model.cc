// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_model.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/metrics/metrics_hashes.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/language/core/common/language_util.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_util.h"

namespace {

// The number of characters to sample and provide as a buffer to the model
// for determining its language.
constexpr size_t kTextSampleLength = 256;

// The number of samples of |kTextSampleLength| to evaluate the model when
// determining the language of the page content.
constexpr int kNumTextSamples = 3;

}  // namespace

namespace translate {
// If enabled, the string passed to the language detection model for the whole
// page is truncated to `kTextSampleLength`
BASE_FEATURE(kTruncateLanguageDetectionSample,
             "TruncateLanguageDetectionSample",
             base::FEATURE_DISABLED_BY_DEFAULT);

LanguageDetectionModel::LanguageDetectionModel(
    language_detection::LanguageDetectionModel& shared_tflite_model)
    : tflite_model_(shared_tflite_model) {}

LanguageDetectionModel::LanguageDetectionModel(
    std::unique_ptr<language_detection::LanguageDetectionModel>
        owned_tflite_model)
    : owned_tflite_model_(std::move(owned_tflite_model)),
      tflite_model_(*owned_tflite_model_.get()) {
  owned_tflite_model_->DetachFromSequence();
}

LanguageDetectionModel::~LanguageDetectionModel() = default;

void LanguageDetectionModel::UpdateWithFile(base::File model_file) {
  tflite_model_->UpdateWithFile(std::move(model_file));
}

void LanguageDetectionModel::UpdateWithFileAsync(base::File model_file,
                                                 base::OnceClosure callback) {
  tflite_model_->UpdateWithFileAsync(std::move(model_file),
                                     std::move(callback));
}

bool LanguageDetectionModel::IsAvailable() const {
  return tflite_model_->IsAvailable();
}

std::pair<std::string, float> LanguageDetectionModel::DetectTopLanguage(
    const std::u16string& sampled_str) const {
  TRACE_EVENT("browser", "LanguageDetectionModel::DetectTopLanguage");

  auto categories =
      tflite_model_->Predict(sampled_str,
                             /*truncate=*/base::FeatureList::IsEnabled(
                                 kTruncateLanguageDetectionSample));
  auto top_category = language_detection::TopPrediction((categories));
  base::UmaHistogramSparse(
      "LanguageDetection.TFLiteModel.ClassifyText.HighestConfidenceLanguage",
      base::HashMetricName(top_category.language));
  return std::make_pair(top_category.language, top_category.score);
}

std::string LanguageDetectionModel::DeterminePageLanguage(
    const std::string& code,
    const std::string& html_lang,
    const std::u16string& contents,
    std::string* predicted_language,
    bool* is_prediction_reliable,
    float& prediction_reliability_score) const {
  DCHECK(IsAvailable());

  if (!predicted_language || !is_prediction_reliable) {
    return translate::kUnknownLanguageCode;
  }

  *is_prediction_reliable = false;
  *predicted_language = translate::kUnknownLanguageCode;
  prediction_reliability_score = 0.0;

  if (!tflite_model_->IsAvailable()) {
    return translate::kUnknownLanguageCode;
  }

  const language_detection::Prediction prediction = DetectLanguage(contents);
  prediction_reliability_score = prediction.score;

  // TODO(crbug.com/40748826): Use the model threshold provided
  // by the model itself. Not needed until threshold is finalized.
  bool is_reliable =
      prediction_reliability_score > GetTFLiteLanguageDetectionThreshold();

  std::string final_prediction = translate::FilterDetectedLanguage(
      base::UTF16ToUTF8(contents), prediction.language, is_reliable);
  *predicted_language = final_prediction;
  *is_prediction_reliable = is_reliable;
  language::ToTranslateLanguageSynonym(&final_prediction);

  LOCAL_HISTOGRAM_BOOLEAN("LanguageDetection.TFLite.DidAttemptDetection", true);
  return translate::DeterminePageLanguage(code, html_lang, final_prediction,
                                          is_reliable);
}

language_detection::Prediction LanguageDetectionModel::DetectLanguage(
    const std::u16string& contents) const {
  base::ElapsedTimer timer;
  if (!tflite_model_->IsAvailable()) {
    return language_detection::Prediction{translate::kUnknownLanguageCode,
                                          0.0f};
  }

  std::vector<std::pair<std::string, float>> model_predictions;
  // First evaluate the model on the entire contents based on the model's
  // implementation, for v1 it is the first 128 tokens that are unicode
  // "letters". We do not need to have the model's length in sync with
  // the sampling logic for v1 as 128 tokens is unlikely to be changed.
  model_predictions.emplace_back(DetectTopLanguage(contents));
  if (contents.length() > kNumTextSamples * kTextSampleLength) {
    // Strings with UTF-8 have different widths so substr should be performed on
    // the UTF16 strings to ensure alignment and then convert down to UTF-8
    // strings for model evaluation.
    std::u16string sampled_str = contents.substr(
        contents.length() - kTextSampleLength, kTextSampleLength);
    // Evaluate on the last |kTextSampleLength| characters.
    model_predictions.emplace_back(DetectTopLanguage(sampled_str));

    // Sample and evaluate on the middle |kTextSampleLength| characters.
    sampled_str = contents.substr(contents.length() / 2, kTextSampleLength);
    model_predictions.emplace_back(DetectTopLanguage(sampled_str));
  }

  const auto top_language_result =
      std::max_element(model_predictions.begin(), model_predictions.end());
  base::UmaHistogramTimes(
      "LanguageDetection.TFLiteModel.DetectPageLanguage.Duration",
      timer.Elapsed());
  base::UmaHistogramCounts1M(
      "LanguageDetection.TFLiteModel.DetectPageLanguage.Size", contents.size());

  return language_detection::Prediction{top_language_result->first,
                                        top_language_result->second};
}

std::string LanguageDetectionModel::GetModelVersion() const {
  return tflite_model_->GetModelVersion();
}

}  // namespace translate
