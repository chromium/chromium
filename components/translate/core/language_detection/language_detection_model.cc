// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_model.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/metrics/metrics_hashes.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/language/core/common/language_util.h"
#include "components/language_detection/core/constants.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_util.h"

namespace translate {
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

std::string LanguageDetectionModel::DeterminePageLanguage(
    const std::string& code,
    const std::string& html_lang,
    const std::u16string& contents,
    std::string* predicted_language,
    bool* is_prediction_reliable,
    float& prediction_reliability_score) const {
  DCHECK(IsAvailable());

  if (!predicted_language || !is_prediction_reliable) {
    return language_detection::kUnknownLanguageCode;
  }

  *is_prediction_reliable = false;
  *predicted_language = language_detection::kUnknownLanguageCode;
  prediction_reliability_score = 0.0;

  if (!tflite_model_->IsAvailable()) {
    return language_detection::kUnknownLanguageCode;
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
  auto prediction = tflite_model_->PredictTopLanguageWithSamples(contents);
  base::UmaHistogramTimes(
      "LanguageDetection.TFLiteModel.DetectPageLanguage.Duration",
      timer.Elapsed());
  base::UmaHistogramCounts1M(
      "LanguageDetection.TFLiteModel.DetectPageLanguage.Size", contents.size());

  return prediction;
}

std::string LanguageDetectionModel::GetModelVersion() const {
  return tflite_model_->GetModelVersion();
}

}  // namespace translate
